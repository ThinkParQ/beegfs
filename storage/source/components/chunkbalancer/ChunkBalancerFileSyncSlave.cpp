#include <app/App.h>
#include <toolkit/StorageTkEx.h>
#include <program/Program.h>
#include <common/net/message/storage/chunkbalancing/UpdateStripePatternMsg.h>
#include <common/net/message/storage/chunkbalancing/UpdateStripePatternRespMsg.h>
#include <common/net/message/storage/creating/RmChunkPathsMsg.h>
#include <common/net/message/storage/creating/RmChunkPathsRespMsg.h>
#include <common/storage/FileEvent.h>
#include "ChunkBalancerFileSyncSlave.h"

ChunkBalancerFileSyncSlave::ChunkBalancerFileSyncSlave(uint16_t targetID,
   ChunkSyncCandidateStore* syncCandidates, uint8_t  slaveID) :  ChunkFileResyncer(targetID,  slaveID)

{
   this->chunkFileResyncerMode = CHUNKFILERESYNCER_FLAG_CHUNKBALANCE; // chunk should not be copied to buddymir dir 
   this->syncCandidates = syncCandidates;  
   this->targetID = targetID;             
}

ChunkBalancerFileSyncSlave::~ChunkBalancerFileSyncSlave()
{
}

void ChunkBalancerFileSyncSlave::syncLoop()
{
   const char* logContext = "ChunkBalanceFileSyncSlave running";

   while (!getSelfTerminateNotIdle())
   {
      if ((syncCandidates->isFilesEmpty())) 
         continue;

      ChunkSyncCandidateFile candidate;

      syncCandidates->fetch(candidate, this);

      if (unlikely(candidate.getTargetID() == 0)) // ignore targetID 0
         continue;

      std::string relativePath = candidate.getRelativePath();
      uint16_t localTargetID = candidate.getTargetID();
      uint16_t destinationID = candidate.getDestinationID();
      uint16_t destinationTargetID = destinationID; //in case of mirrored chunk, need to distinguish between mirrorGroupID and targetID

      EntryInfo* entryInfo = candidate.getEntryInfo();
      FileEvent* fileEvent = candidate.getFileEvent();
      isBuddyMirrorChunk = candidate.isChunkBuddyMirrored();

      App* app = Program::getApp();
      NodeStoreServers* metaNodes = app->getMetaNodes();
      NumNodeID metaNodeID;
      ChunkStore* chunkDirStore = app->getChunkDirStore();
      const auto& target = app->getStorageTargets()->getTargets().at(localTargetID);
      MirrorBuddyGroupMapper* buddyGroupMapper = app->getMirrorBuddyGroupMapper();
      MirrorBuddyGroupMapper* metaMirrorGroupMapper = app->getMetaMirrorBuddyGroupMapper();
      std::string entryID = StorageTk::getPathBasename(relativePath);

      if (entryID!= entryInfo->getEntryID())
      {
         LogContext(logContext).log(LogTopic_CHUNKBALANCING, Log_WARNING,
            "EntryID of chunk does not match the one from path, stopping copy chunk operation. The following chunk is not balanced. chunkPath: " 
            + relativePath  + "; entryID: " + entryInfo->getEntryID() + "; targetID: " + std::to_string(localTargetID)+ "; destinationID: " + std::to_string(destinationTargetID)); 
            errorCount.increase();
            continue;
      }

      // get meta node id of chunk
      if ( entryInfo->getIsBuddyMirrored() )
         metaNodeID = NumNodeID(
            metaMirrorGroupMapper->getPrimaryTargetID(entryInfo->getOwnerNodeID().val() ) );

      else
         metaNodeID = entryInfo->getOwnerNodeID();

      NodeHandle metaNode = metaNodes->referenceNode(metaNodeID);
      if (!metaNode)
      {
            LogContext(logContext).log(LogTopic_CHUNKBALANCING, Log_WARNING,
            "Reference of meta node failed. chunkPath: " + relativePath + "; targetID: "
               + std::to_string(localTargetID));
                  errorCount.increase();
            continue;
      }
      
      if (isBuddyMirrorChunk)
      {
         chunkFileResyncerMode = CHUNKFILERESYNCER_FLAG_CHUNKBALANCE_MIRRORED;
      }
      else 
      {
         chunkFileResyncerMode = CHUNKFILERESYNCER_FLAG_CHUNKBALANCE; 
      }
      // perform chunk sync
      LOG_DEBUG(logContext, Log_SPAM,  "Copy chunk operation started. chunkPath: " 
         + relativePath  + "; entryID: " + entryInfo->getEntryID() + "; targetID: " + std::to_string(localTargetID)+ "; destinationID: " + std::to_string(destinationTargetID));
 
      FhgfsOpsErr resyncRes = ChunkFileResyncer::doResync(relativePath, localTargetID, destinationID, chunkFileResyncerMode);
      if (resyncRes != FhgfsOpsErr_SUCCESS && resyncRes != FhgfsOpsErr_PATHNOTEXISTS)
      {
         LogContext(logContext).log(LogTopic_CHUNKBALANCING, Log_WARNING,
            "Resync of chunk failed. chunkPath: " + relativePath + "; targetID: "
               + std::to_string(localTargetID));
         //do not exit loop here, need to propagate the error to meta node
      }
      
      //if mirrored, replace targetIDs with mirror groupIDs
      if (isBuddyMirrorChunk)
      {
         localTargetID = buddyGroupMapper->getBuddyGroupID(localTargetID);
         destinationID = buddyGroupMapper->getBuddyGroupID(destinationID);
      }

      UpdateStripePatternMsg patternUpdateMsg(localTargetID, destinationID, entryInfo, &relativePath, resyncRes, fileEvent);
      patternUpdateMsg.addMsgHeaderFeatureFlag(UPDATESTRIPEPATTERNMSG_FLAG_HAS_EVENT);

      const auto patternUpdateMsgResp = MessagingTk::requestResponse(*metaNode, patternUpdateMsg, NETMSGTYPE_UpdateStripePatternResp);
      if (!patternUpdateMsgResp)
      {
         LogContext(logContext).log(LogTopic_CHUNKBALANCING, Log_WARNING,
            "Communication with metadata server failed." +
            (*metaNode).getNodeIDWithTypeStr()+ " ; ");
         errorCount.increase();
         continue;
      }
      // correct response type received
      UpdateStripePatternRespMsg* UpdateStripePatternRespMsgCast = (UpdateStripePatternRespMsg*)patternUpdateMsgResp.get();
      FhgfsOpsErr stripeUpdateRes = UpdateStripePatternRespMsgCast->getResult();
      if (stripeUpdateRes != FhgfsOpsErr_SUCCESS)
      {
         LogContext(logContext).log(LogTopic_CHUNKBALANCING, Log_WARNING,
            "Stripe pattern update of chunk failed. There may be orphaned chunks in the system, "
            "you may want to perform a filesystem check to ensure all additional chunks are deleted. chunkPath: " 
            + relativePath + "; targetID: " + std::to_string(localTargetID));
         errorCount.increase();
         continue;
      }

      if (resyncRes != FhgfsOpsErr_SUCCESS)
      //resync of chunk failed, but stripe pattern update was SUCCESS
      //no need to remove original chunk since it should not exist on storage, exit here
      {
         numChunksSynced.increase();
         continue;
      }

      if (isBuddyMirrorChunk)
      {
         //send RmChunkPathsMsg to source secondary storage to remove original chunk
         uint16_t secondaryID = buddyGroupMapper->getSecondaryTargetID(localTargetID);
         bool secondaryRes = sendRemoveChunkPathsMessage(secondaryID, relativePath, isBuddyMirrorChunk);
         if (!secondaryRes)
         {   
            LogContext(logContext).log(LogTopic_CHUNKBALANCING, Log_WARNING,
            "Removal of chunk on secondary buddy failed. There may be orphaned chunks in the system, "
            "you may want to perform a filesystem check to ensure all additional chunks are deleted. chunkPath: " 
            + relativePath + "; targetID: " + std::to_string(localTargetID));
            errorCount.increase(); 
            continue;
         }
      }
      // remove original chunk on source storage
      int fd = getFD(target);
      FhgfsOpsErr retVal = removeChunk(fd,relativePath);
      if (retVal != FhgfsOpsErr_SUCCESS)
      {   
         LogContext(logContext).log(LogTopic_CHUNKBALANCING, Log_WARNING,
            "Removal of chunk source target failed. There may be orphaned chunks in the system, "
            "you may want to perform a filesystem check to ensure all additional chunks are deleted. chunkPath: " 
            + relativePath + "; targetID: " + std::to_string(localTargetID));
         errorCount.increase();
         continue;
      }
      // removal succeeded
      numChunksSynced.increase();
      // try to remove parent directory if this was the only chunk file in it
      Path parentDirPath(StorageTk::getPathDirname(relativePath));
      chunkDirStore->rmdirChunkDirPath(fd, &parentDirPath);

   }
}

int ChunkBalancerFileSyncSlave::getFD(const std::unique_ptr<StorageTarget> & target)
{
   if (isBuddyMirrorChunk)
      return *target->getMirrorFD();
   else 
      return *target->getChunkFD();
}

FhgfsOpsErr ChunkBalancerFileSyncSlave::removeChunk(int& targetFD, std::string& relativePath)
{
   int unlinkRes = unlinkat(targetFD, relativePath.c_str(), 0);
   if ( (unlinkRes != 0)  && (errno != ENOENT) )
   {
      LogContext(__func__).log(LogTopic_CHUNKBALANCING, Log_WARNING,
         "Removal of chunk failed. chunkPath: " + relativePath + "; targetfD: "
            + std::to_string(targetFD)+ "; error code: " + std::to_string(errno) );
      return FhgfsOpsErr_INTERNAL;
   }
   return FhgfsOpsErr_SUCCESS;
}

bool ChunkBalancerFileSyncSlave::sendRemoveChunkPathsMessage(uint16_t& targetID, std::string& relativePath, bool isBuddyMirrorChunk)
{   
   bool retVal = true;
   unsigned msgRetryIntervalMS = 5000;
   // try to find the node with the TargetID
   App* app = Program::getApp();
   TargetMapper* targetMapper = app->getTargetMapper();
   NodeStoreServers* storageNodes = app->getStorageNodes();
   NumNodeID nodeID = targetMapper->getNodeID(targetID);
   auto node = storageNodes->referenceNode(nodeID);
   if (!node)
   {
      LogContext(__func__).log(Log_WARNING,
         "Storage node does not exist; nodeID " + nodeID.str());
      retVal = false;
      return retVal;
   }
   
   StringList rmPaths;
   rmPaths.push_back(relativePath);
   RmChunkPathsMsg rmMsg(targetID, &rmPaths);
   if (isBuddyMirrorChunk)
      rmMsg.addMsgHeaderFeatureFlag(RMCHUNKPATHSMSG_FLAG_BUDDYMIRROR); 
   rmMsg.setMsgHeaderTargetID(targetID);

   CombinedTargetState state;
   bool getStateRes = Program::getApp()->getTargetStateStore()->getState(targetID, state);

   // send request to node and receive response
   std::unique_ptr<NetMessage> respMsg;

   while ( (!respMsg) && (getStateRes)
      && (state.reachabilityState != TargetReachabilityState_OFFLINE) )
   {
      respMsg = MessagingTk::requestResponse(*node, rmMsg, NETMSGTYPE_RmChunkPathsResp);

      if (!respMsg)
      {
         LOG_DEBUG(__func__, Log_NOTICE,
            "Unable to communicate, but target is not offline; "
            "sleeping " + std::to_string(msgRetryIntervalMS) + " ms before retry. "
            "targetID: " + std::to_string(targetID) );

         PThread::sleepMS(msgRetryIntervalMS);

         // if thread shall terminate, break loop here
         if ( getSelfTerminateNotIdle() )
            break;

         getStateRes = Program::getApp()->getTargetStateStore()->getState(targetID, state);
      }
   }

   if (!respMsg)
   { // communication error
      LogContext(__func__).logErr(
         "Communication with storage node failed: " + (*node).getTypedNodeID() );

      retVal = false;
      return retVal;
   }
   else
   if (!getStateRes)
   {
      LogContext(__func__).log(Log_WARNING,
         "No valid state for node ID: " + (*node).getTypedNodeID() );

      retVal = false;
      return retVal;
   }
   else
   {
      // correct response type received
      RmChunkPathsRespMsg* respMsgCast = (RmChunkPathsRespMsg*) respMsg.get();
      StringList& failedPaths = respMsgCast->getFailedPaths();

      for (StringListIter iter = failedPaths.begin(); iter != failedPaths.end(); iter++)
      {
         LogContext(__func__).logErr("Chunk path could not be deleted; "
            "path: " + *iter + "; "
            "targetID: " + std::to_string(targetID) + "; "
            "node: " + (*node).getTypedNodeID());
         retVal = false;
      }
   }
   return retVal;
}

 
