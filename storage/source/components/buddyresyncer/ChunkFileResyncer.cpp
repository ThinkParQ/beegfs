#include <common/net/message/storage/creating/RmChunkPathsMsg.h>
#include <common/net/message/storage/creating/RmChunkPathsRespMsg.h>
#include <common/net/message/storage/mirroring/ResyncLocalFileMsg.h>
#include <common/net/message/storage/mirroring/ResyncLocalFileRespMsg.h> 
#include <toolkit/StorageTkEx.h>
#include <program/Program.h>
#include "ChunkFileResyncer.h"

#include <boost/lexical_cast.hpp>

#define PROCESS_AT_ONCE 1
#define SYNC_BLOCK_SIZE (1024*1024) // 1M

ChunkFileResyncer::ChunkFileResyncer(uint16_t targetID, 
         uint8_t  slaveID) :
   PThread("ChunkFileResyncer_" + std::to_string(targetID) + "-"
      + std::to_string(slaveID))
{
   this->isRunning = false;
   this->targetID = targetID;
}

ChunkFileResyncer::~ChunkFileResyncer()
{
}
 
/**
 * This is a component, which is started through its control frontend on-demand at
 * runtime and terminates when it's done.
 * We have to ensure (in cooperation with the control frontend) that we don't get multiple instances
 * of this thread running at the same time.
 */
void ChunkFileResyncer::run()
{
   setIsRunning(true);

   try
   {
      LogContext(__func__).log(Log_DEBUG, "Component started.");

      registerSignalHandler();

      numChunksSynced.setZero(); 
      errorCount.setZero();

      syncLoop();

      LogContext(__func__).log(Log_DEBUG, "Component stopped.");
   }
   catch (std::exception& e)
   {
      PThread::getCurrentThreadApp()->handleComponentException(e);
   }

   setIsRunning(false);
}

FhgfsOpsErr ChunkFileResyncer::doResync(std::string& chunkPathStr, uint16_t localTargetID,
   uint16_t destinationTargetID, ChunkFileResyncerMode chunkFileResyncerMode)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;
   unsigned msgRetryIntervalMS = 5000;

   App* app = Program::getApp();
   TargetMapper* targetMapper = app->getTargetMapper();
   NodeStoreServers* storageNodes = app->getStorageNodes();
   ChunkLockStore* chunkLockStore = app->getChunkLockStore();

   std::string entryID = StorageTk::getPathBasename(chunkPathStr);

   // try to find the node with the destination TargetID
   NumNodeID destNodeID = targetMapper->getNodeID(destinationTargetID);

   auto node = storageNodes->referenceNode(destNodeID);

   if (!node)
   {
      LogContext(__func__).log(Log_WARNING,
         "Storage node does not exist; nodeID " + destNodeID.str());
      return FhgfsOpsErr_UNKNOWNNODE;
   }

   int64_t offset = 0;
   ssize_t readRes = 0;
   unsigned resyncMsgFlags = 0;

   LogContext(__func__).log(Log_DEBUG,
      "Copy chunk operation started. chunkPath: " + chunkPathStr + "; localTargetID: "
         + std::to_string(localTargetID) + "; destinationTargetID: "
         + std::to_string(destinationTargetID)+ "; chunkFileResyncerMode: "
         + std::to_string(chunkFileResyncerMode));

   do
   {
      if (chunkFileResyncerMode == CHUNKFILERESYNCER_FLAG_BUDDYMIRROR)
      {
         resyncMsgFlags |= RESYNCLOCALFILEMSG_FLAG_BUDDYMIRROR; //set buddy mirroring flag 
      } 
      else if (chunkFileResyncerMode == CHUNKFILERESYNCER_FLAG_CHUNKBALANCE_MIRRORED)
      {
         resyncMsgFlags |= RESYNCLOCALFILEMSG_FLAG_BUDDYMIRROR;
         resyncMsgFlags |= RESYNCLOCALFILEMSG_FLAG_CHUNKBALANCE_BUDDYMIRROR;
      }
      
      boost::scoped_array<char> data(new char[SYNC_BLOCK_SIZE]);

      const auto& target = app->getStorageTargets()->getTargets().at(localTargetID);

      // lock the chunk
      chunkLockStore->lockChunk(localTargetID, entryID);
      
      fd = openat(getFD(target), chunkPathStr.c_str(), O_RDONLY | O_NOATIME);
   
      if (fd == -1)
      {
         int errCode = errno;

         if (errCode == ENOENT)
         {  
            if (chunkFileResyncerMode == CHUNKFILERESYNCER_FLAG_BUDDYMIRROR) //should not trigger in case of chunk balancing)
            {
               // chunk was deleted => no error
               // delete the chunk and return
               bool rmRes = removeChunkUnlocked(*node, localTargetID, destinationTargetID, chunkPathStr);

               if (!rmRes) // rm failed; stop resync
               {
                  LogContext(__func__).log(Log_WARNING,
                     "File sync not started. chunkPath: " + chunkPathStr + "; localTargetID: "
                        + std::to_string(localTargetID) + "; destinationTargetID: "
                        + std::to_string(destinationTargetID));

                  retVal = FhgfsOpsErr_INTERNAL;
               }
            }
            else 
            {
               LogContext(__func__).log(Log_DEBUG,
                  "Open of chunk failed, chunk not found on storage target. Expected when rebalancing special, empty or small files. "
                     "chunkPath: " + chunkPathStr + "; targetID: "
                     + std::to_string(localTargetID));

               retVal = FhgfsOpsErr_PATHNOTEXISTS;
            }
         }
         else // error => log and return
         {
            LogContext(__func__).logErr(
               "Open of chunk failed. chunkPath: " + chunkPathStr + "; targetID: "
                  + std::to_string(localTargetID) + "; Error: "
                  + System::getErrString(errCode));

            retVal = FhgfsOpsErr_INTERNAL;
         }

         chunkLockStore->unlockChunk(localTargetID, entryID);

         goto cleanup;
      }

      int seekRes = lseek(fd, offset, SEEK_SET);

      if (seekRes == -1)
      {
         LogContext(__func__).logErr(
            "Seeking in chunk failed. chunkPath: " + chunkPathStr + "; targetID: "
               + std::to_string(localTargetID) + "; offset: " + StringTk::int64ToStr(offset));

         chunkLockStore->unlockChunk(localTargetID, entryID);

         goto cleanup;
      }

      readRes = read(fd, data.get(), SYNC_BLOCK_SIZE);

      if (readRes == -1)
      {
         LogContext(__func__).logErr("Error during read; "
            "chunkPath: " + chunkPathStr + "; "
            "targetID: " + std::to_string(localTargetID) + "; "
            "TargetNode: " + node->getTypedNodeID() + "; "
            "DestinationTargetID: " + std::to_string(destinationTargetID) + "; "
            "Error: " + System::getErrString(errno));

         retVal = FhgfsOpsErr_INTERNAL;

         goto end_of_loop;
      }

      if (readRes > 0)
      {
         const char zeroBuf[RESYNCER_SPARSE_BLOCK_SIZE] = { 0 };

         // check if sparse blocks are in the buffer
         ssize_t bufPos = 0;
         bool dataFound = false;
         while (bufPos < readRes)
         {
            size_t cmpLen = BEEGFS_MIN(readRes-bufPos, RESYNCER_SPARSE_BLOCK_SIZE);

            int cmpRes = memcmp(data.get() + bufPos, zeroBuf, cmpLen);
            if (cmpRes != 0)
               dataFound = true;
            else // sparse area detected
            {
               if (dataFound) // had data before
               {
                  resyncMsgFlags |= RESYNCLOCALFILEMSG_CHECK_SPARSE; // let the receiver do a check
                  break; // and stop checking here
               }
            }

            bufPos += cmpLen;
         }

         // this inner loop is over and there are only sparse areas

         /* make sure we always send a msg at offset==0 to truncate the file and allow concurrent
            writers in a big inital sparse area */
         if (offset && (readRes > 0) && (readRes == SYNC_BLOCK_SIZE) && !dataFound)
         {
            goto end_of_loop;
            // => no transfer needed
         }

         /* let the receiver do a check, because we might be sending a sparse block at beginnig or
            end of file */
         if (!dataFound)
            resyncMsgFlags |= RESYNCLOCALFILEMSG_CHECK_SPARSE;
      }

      {
         ResyncLocalFileMsg resyncMsg(data.get(), chunkPathStr, destinationTargetID, offset, readRes);

         if (!readRes || (readRes < SYNC_BLOCK_SIZE) ) // last iteration, set attribs and trunc buddy chunk
         {
            struct stat statBuf;
            int statRes = fstat(fd, &statBuf);

            if (statRes == 0)
            {
               if (statBuf.st_size < offset)
               { // in case someone truncated the file while we're reading at a high offset
                  offset = statBuf.st_size;
                  resyncMsg.setOffset(offset);
               }
               else
               if (offset && !readRes)
                  resyncMsgFlags |= RESYNCLOCALFILEMSG_FLAG_TRUNC;

               int mode = statBuf.st_mode;
               unsigned userID = statBuf.st_uid;
               unsigned groupID = statBuf.st_gid;
               int64_t mtimeSecs = statBuf.st_mtim.tv_sec;
               int64_t atimeSecs = statBuf.st_atim.tv_sec;
               SettableFileAttribs chunkAttribs = {mode, userID,groupID, mtimeSecs, atimeSecs};
               resyncMsg.setChunkAttribs(chunkAttribs);
               resyncMsgFlags |= RESYNCLOCALFILEMSG_FLAG_SETATTRIBS;
            }
            else
            {
               LogContext(__func__).logErr("Error getting chunk attributes; "
                  "chunkPath: " + chunkPathStr + "; "
                  "targetID: " + std::to_string(localTargetID) + "; "
                  "destinationNode: " + node->getTypedNodeID() + "; "
                  "destinationTargetID: " + std::to_string(destinationTargetID) + "; "
                  "Error: " + System::getErrString(errno));
            }
         }

         resyncMsg.setMsgHeaderFeatureFlags(resyncMsgFlags);
         resyncMsg.setMsgHeaderTargetID(destinationTargetID);

         CombinedTargetState state;
         bool getStateRes =
            Program::getApp()->getTargetStateStore()->getState(destinationTargetID, state);

         // send request to node and receive response
         std::unique_ptr<NetMessage> respMsg;

         while ( (!respMsg) && (getStateRes)
            && (state.reachabilityState != TargetReachabilityState_OFFLINE) )
         {
            respMsg = MessagingTk::requestResponse(*node, resyncMsg,
                  NETMSGTYPE_ResyncLocalFileResp);

            if (!respMsg)
            {
               LOG_DEBUG(__func__, Log_NOTICE,
                  "Unable to communicate, but target is not offline; sleeping "
                  + std::to_string(msgRetryIntervalMS) + "ms before retry. targetID: "
                  + std::to_string(localTargetID));

               PThread::sleepMS(msgRetryIntervalMS);

               // if thread shall terminate, break loop here
               if ( getSelfTerminateNotIdle() )
                  break;

               getStateRes =
                  Program::getApp()->getTargetStateStore()->getState(destinationTargetID, state);
            }
         }

         if (!respMsg)
         { // communication error
            LogContext(__func__).log(Log_WARNING,
               "Communication with storage node failed: " + node->getTypedNodeID());

            retVal = FhgfsOpsErr_COMMUNICATION;

            // set readRes to non-zero to force exiting loop
            readRes = -2;
         }
         else
         if (!getStateRes)
         {
            LogContext(__func__).log(Log_WARNING,
               "No valid state for node ID: " + node->getTypedNodeID());

            retVal = FhgfsOpsErr_INTERNAL;

            // set readRes to non-zero to force exiting loop
            readRes = -2;
         }
         else
         {
            // correct response type received
            ResyncLocalFileRespMsg* respMsgCast = (ResyncLocalFileRespMsg*) respMsg.get();

            FhgfsOpsErr syncRes = respMsgCast->getResult();

            if (syncRes != FhgfsOpsErr_SUCCESS)
            {
               LogContext(__func__).log(Log_WARNING, "Error during resync; "
                  "chunkPath: " + chunkPathStr + "; "
                  "targetID: " + std::to_string(localTargetID) + "; "
                  "Destination Node: " + node->getTypedNodeID() + "; "
                  "DestinationTargetID: " + std::to_string(destinationTargetID) + "; "
                  "Error: " + boost::lexical_cast<std::string>(syncRes));

               retVal = syncRes;

               // set readRes to non-zero to force exiting loop
               readRes = -2;
            }
         }
      }

   end_of_loop:
      int closeRes = close(fd);
      if (closeRes == -1)
      {
         LogContext(__func__).log(Log_WARNING, "Error closing file descriptor; "
            "chunkPath: " + chunkPathStr + "; "
            "targetID: " + std::to_string(localTargetID) + "; "
            "BuddyNode: " + node->getTypedNodeID() + "; "
            "buddyTargetID: " + std::to_string(destinationTargetID) + "; "
            "Error: " + System::getErrString(errno));
      }
      // unlock the chunk
      chunkLockStore->unlockChunk(localTargetID, entryID);

      // increment offset for next iteration
      offset += readRes;

      if ( getSelfTerminateNotIdle() )
      {
         retVal = FhgfsOpsErr_INTERRUPTED;
         break;
      }

   } while (readRes == SYNC_BLOCK_SIZE);

cleanup:
   LogContext(__func__).log(Log_DEBUG, "File sync finished. chunkPath: " + chunkPathStr);

   return retVal;
}

/**
 * Note: Chunk has to be locked by caller.
 */
bool ChunkFileResyncer::removeChunkUnlocked(Node& node, uint16_t localTargetID, uint16_t destinationTargetID, std::string& pathStr)
{
   bool retVal = true;
   unsigned msgRetryIntervalMS = 5000;

   std::string entryID = StorageTk::getPathBasename(pathStr);
   StringList rmPaths;
   rmPaths.push_back(pathStr);

   RmChunkPathsMsg rmMsg(destinationTargetID, &rmPaths);
   rmMsg.addMsgHeaderFeatureFlag(RMCHUNKPATHSMSG_FLAG_BUDDYMIRROR);
   
   rmMsg.setMsgHeaderTargetID(destinationTargetID);

   CombinedTargetState state;
   bool getStateRes = Program::getApp()->getTargetStateStore()->getState(destinationTargetID, state);

   // send request to node and receive response
   std::unique_ptr<NetMessage> respMsg;

   while ( (!respMsg) && (getStateRes)
      && (state.reachabilityState != TargetReachabilityState_OFFLINE) )
   {
      respMsg = MessagingTk::requestResponse(node, rmMsg, NETMSGTYPE_RmChunkPathsResp);

      if (!respMsg)
      {
         LOG_DEBUG(__func__, Log_NOTICE,
            "Unable to communicate, but target is not offline; "
            "sleeping " + std::to_string(msgRetryIntervalMS) + " ms before retry. "
            "targetID: " + std::to_string(localTargetID) );

         PThread::sleepMS(msgRetryIntervalMS);

         // if thread shall terminate, break loop here
         if ( getSelfTerminateNotIdle() )
            break;

         getStateRes = Program::getApp()->getTargetStateStore()->getState(destinationTargetID, state);
      }
   }

   if (!respMsg)
   { // communication error
      LogContext(__func__).logErr(
         "Communication with storage node failed: " + node.getTypedNodeID() );

      return false;
   }
   else
   if (!getStateRes)
   {
      LogContext(__func__).log(Log_WARNING,
         "No valid state for node ID: " + node.getTypedNodeID() );

      return false;
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
            "targetID: " + std::to_string(localTargetID) + "; "
            "node: " + node.getTypedNodeID());
         retVal = false;
      }
   }

   return retVal;
}
