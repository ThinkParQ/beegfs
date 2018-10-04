#include <common/net/message/storage/attribs/GetChunkFileAttribsMsg.h>
#include <common/net/message/storage/attribs/GetChunkFileAttribsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <components/worker/GetChunkFileAttribsWork.h>
#include <program/Program.h>
#include "MsgHelperStat.h"


/**
 * Note: This will automatically refresh dynamic attribs if they are outdated.
 *
 * @param loadFromDisk do we need to load the data from disk or do we want to have data from
 *    an already opened inode only
 * @param msgUserID will only be used in msg header info.
 * @param outParentNodeID may be NULL (default) if the caller is not interested
 * @param outParentEntryID may NULL (if outParentNodeID is NULL)
 */
FhgfsOpsErr MsgHelperStat::stat(EntryInfo* entryInfo, bool loadFromDisk, unsigned msgUserID,
   StatData& outStatData, NumNodeID* outParentNodeID, std::string* outParentEntryID)
{
   const char* logContext = "Stat Helper (stat entry)";

   MetaStore* metaStore = Program::getApp()->getMetaStore();

   FhgfsOpsErr retVal;

   retVal = metaStore->stat(entryInfo, loadFromDisk, outStatData, outParentNodeID,
      outParentEntryID);

   if(retVal == FhgfsOpsErr_DYNAMICATTRIBSOUTDATED)
   { // dynamic attribs outdated => get fresh dynamic attribs from storage servers and stat again
      MsgHelperStat::refreshDynAttribs(entryInfo, false, msgUserID);

      //note: if we are here it is regular file and we don't need to request parentData

      retVal = metaStore->stat(entryInfo, loadFromDisk, outStatData);

      // this time we ignore outdated dynamic attribs because refreshing them again would
      // be useless (or we could keep on doing it forever)
      if(retVal == FhgfsOpsErr_DYNAMICATTRIBSOUTDATED)
         retVal = FhgfsOpsErr_SUCCESS;
   }
   else
   if(retVal == FhgfsOpsErr_PATHNOTEXISTS && loadFromDisk)
   { /* metadata not found: it is hard to tell whether this is an error (e.g. metadata was never
        created) or just a normal case (e.g. someone removed a file during an "ls -l") */

      LogContext(logContext).log(Log_DEBUG, "Missing metadata for entryID: " +
         entryInfo->getEntryID() + ". "
         "(Possibly a valid race of two processes or a cached entry that is now being "
         "checked by a client revalidate() method.)");
   }

   return retVal;
}


/**
 * Refresh current file size and other dynamic attribs from storage servers.
 *
 * @makePersistent whether or not this method should also update persistent metadata.
 * @param msgUserID only used for msg header info.
 */
FhgfsOpsErr MsgHelperStat::refreshDynAttribs(EntryInfo* entryInfo, bool makePersistent,
   unsigned msgUserID)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   FhgfsOpsErr retVal;

   std::string parentEntryID = entryInfo->getParentEntryID();
   std::string entryID       = entryInfo->getEntryID();

   MetaFileHandle inode = metaStore->referenceFile(entryInfo);
   if(!inode)
   {
      std::string logContext("Stat Helper (refresh filesize:  parentID: " + parentEntryID
         + " entryID: " + entryID + ")");
      LogContext(logContext).log(Log_DEBUG, std::string("File not exists") );

      return FhgfsOpsErr_PATHNOTEXISTS;
   }

   if(inode->getStripePattern()->getAssignedNumTargets() == 1)
      retVal = refreshDynAttribsSequential(*inode, entryID, msgUserID);
   else
      retVal = refreshDynAttribsParallel(*inode, entryID, msgUserID);

   if( (retVal == FhgfsOpsErr_SUCCESS) && makePersistent)
   {
      bool persistenceRes = inode->updateInodeOnDisk(entryInfo);
      if(!persistenceRes)
         retVal = FhgfsOpsErr_INTERNAL;
   }

   metaStore->releaseFile(entryInfo->getParentEntryID(), inode);

   return retVal;
}

/**
 * Note: This method does support getting attrs from buddymirrors, but only from group's primary.
 *
 * @param msgUserID only used for msg header info.
 */
FhgfsOpsErr MsgHelperStat::refreshDynAttribsSequential(FileInode& inode, const std::string& entryID,
   unsigned msgUserID)
{
   const char* logContext = "Stat Helper (refresh chunk files S)";

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS; // will be set to node error, if any

   App* app = Program::getApp();

   StripePattern* pattern = inode.getStripePattern();
   const UInt16Vector* targetIDs = pattern->getStripeTargetIDs();

   DynamicFileAttribsVec dynAttribsVec(targetIDs->size() );

   PathInfo pathInfo;
   inode.getPathInfo(&pathInfo);

   // send request to each node and receive the response message
   unsigned currentStripeNodeIndex = 0;
   for(UInt16VectorConstIter iter = targetIDs->begin();
      iter != targetIDs->end();
      iter++, currentStripeNodeIndex++)
   {
      uint16_t targetID = *iter;

      // prepare request message

      GetChunkFileAttribsMsg getSizeMsg(entryID, *iter, &pathInfo);

      if(pattern->getPatternType() == StripePatternType_BuddyMirror)
         getSizeMsg.addMsgHeaderFeatureFlag(GETCHUNKFILEATTRSMSG_FLAG_BUDDYMIRROR);

      getSizeMsg.setMsgHeaderUserID(msgUserID);

      // prepare communication

      RequestResponseTarget rrTarget(targetID, app->getTargetMapper(), app->getStorageNodes() );

      rrTarget.setTargetStates(app->getTargetStateStore() );

      if(pattern->getPatternType() == StripePatternType_BuddyMirror)
         rrTarget.setMirrorInfo(app->getStorageBuddyGroupMapper(), false);

      RequestResponseArgs rrArgs(NULL, &getSizeMsg, NETMSGTYPE_GetChunkFileAttribsResp);

      // communicate

      FhgfsOpsErr requestRes = MessagingTk::requestResponseTarget(&rrTarget, &rrArgs);

      if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
      { // communication error
         LogContext(logContext).log(Log_WARNING,
            std::string("Communication with storage target failed. ") +
            (pattern->getPatternType() == StripePatternType_BuddyMirror ? "Mirror " : "") +
            "TargetID: " + StringTk::uintToStr(targetID) + "; "
            "EntryID: " + entryID);

         retVal = requestRes;
         continue;
      }

      // correct response type received
      auto* getSizeRespMsg = (GetChunkFileAttribsRespMsg*)rrArgs.outRespMsg.get();

      FhgfsOpsErr getSizeResult = getSizeRespMsg->getResult();
      if(getSizeResult != FhgfsOpsErr_SUCCESS)
      { // error: got no fresh attributes
         LogContext(logContext).log(Log_WARNING,
            std::string("Getting fresh chunk file attributes from target failed. ") +
            (pattern->getPatternType() == StripePatternType_BuddyMirror ? "Mirror " : "") +
            "TargetID: " + StringTk::uintToStr(targetID) + "; "
            "EntryID: " + entryID);

         retVal = getSizeResult;
         continue;
      }

      // success: got fresh chunk file attributes
      //log.log(3, std::string("Got fresh filesize from node: ") + nodeID);

      DynamicFileAttribs currentDynAttribs(getSizeRespMsg->getStorageVersion(),
         getSizeRespMsg->getSize(), getSizeRespMsg->getAllocedBlocks(),
         getSizeRespMsg->getModificationTimeSecs(), getSizeRespMsg->getLastAccessTimeSecs() );

      dynAttribsVec[currentStripeNodeIndex] = currentDynAttribs;
   }


   inode.setDynAttribs(dynAttribsVec); // the actual update

   if(retVal != FhgfsOpsErr_SUCCESS)
   {
      LogContext(logContext).log(Log_WARNING,
         "Problems occurred during chunk file attributes refresh. "
         "EntryID: " + entryID);
   }

   return retVal;
}

/**
 * Note: For buddymirrored files, only group's primary is used.
 */
FhgfsOpsErr MsgHelperStat::refreshDynAttribsParallel(FileInode& inode, const std::string& entryID,
   unsigned msgUserID)
{
   const char* logContext = "Stat Helper (refresh chunk files)";

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS; // will be set to node error, if any

   App* app = Program::getApp();
   MultiWorkQueue* slaveQ = app->getCommSlaveQueue();
   StripePattern* pattern = inode.getStripePattern();
   const UInt16Vector* targetIDs = pattern->getStripeTargetIDs();

   DynamicFileAttribsVec dynAttribsVec(targetIDs->size() );

   size_t numWorks = targetIDs->size();

   FhgfsOpsErrVec nodeResults(numWorks);
   SynchronizedCounter counter;

   PathInfo pathInfo;
   inode.getPathInfo(&pathInfo);

   for(size_t i=0; i < numWorks; i++)
   {
      GetChunkFileAttribsWork* work = new GetChunkFileAttribsWork(entryID, pattern, (*targetIDs)[i],
         &pathInfo, &(dynAttribsVec[i]), &(nodeResults[i]), &counter);

      work->setMsgUserID(msgUserID);

      slaveQ->addDirectWork(work);
   }

   counter.waitForCount(numWorks);

   for(size_t i=0; i < numWorks; i++)
   {
      if(nodeResults[i] != FhgfsOpsErr_SUCCESS)
      {
         LogContext(logContext).log(Log_WARNING,
            "Problems occurred during file attribs refresh. entryID: " + entryID);

         retVal = nodeResults[i];
         break;
      }
   }

   inode.setDynAttribs(dynAttribsVec); // the actual update

   return retVal;
}
