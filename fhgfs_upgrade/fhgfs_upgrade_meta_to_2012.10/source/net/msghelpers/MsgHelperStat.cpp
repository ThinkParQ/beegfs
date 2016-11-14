#include <program/Program.h>
#include <common/net/message/storage/attribs/GetChunkFileAttribsMsg.h>
#include <common/net/message/storage/attribs/GetChunkFileAttribsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <components/worker/GetChunkFileAttribsWork.h>
#include "MsgHelperStat.h"


/**
 * Note: This will automatically refresh dynamic attribs if they are outdated.
 *
 * @param loadFromDisk do we need to load the data from disk or do we want to have data from
 *    an already opened inode only
 */
FhgfsOpsErr MsgHelperStat::stat(EntryInfo* entryInfo, bool loadFromDisk, StatData& outStatData)
{
   const char* logContext = "Stat Helper (stat entry)";

   MetaStore* metaStore = Program::getApp()->getMetaStore();

   FhgfsOpsErr retVal;

   retVal = metaStore->stat(entryInfo, loadFromDisk, outStatData);

   if(retVal == FhgfsOpsErr_DYNAMICATTRIBSOUTDATED)
   { // dynamic attribs outdated => get fresh dynamic attribs from storage servers and stat again
      MsgHelperStat::refreshDynAttribs(entryInfo, false);

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
         "checked by a client revalidate() method.)"); // FIXME Bernd: Not valid if inlined
   }

   return retVal;
}


/**
 * Refresh current file size and other dynamic attribs from storage servers.
 *
 * @makePersistent whether or not this method should also update persistent metadata
 */
FhgfsOpsErr MsgHelperStat::refreshDynAttribs(EntryInfo* entryInfo, bool makePersistent)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   FhgfsOpsErr retVal;

   std::string parentEntryID = entryInfo->getParentEntryID();
   std::string entryID       = entryInfo->getEntryID();

   FileInode* inode = metaStore->referenceFile(entryInfo);
   if(!inode)
   {
      std::string logContext("Stat Helper (refresh filesize:  parentID: " + parentEntryID
         + " entryID: " + entryID + ")");
      LogContext(logContext).log(4, std::string("File not exists") );
      return FhgfsOpsErr_PATHNOTEXISTS;
   }

   if(inode->getStripePattern()->getStripeTargetIDs()->size() == 1)
      retVal = refreshDynAttribsSequential(inode, entryID);
   else
      retVal = refreshDynAttribsParallel(inode, entryID);

   if( (retVal == FhgfsOpsErr_SUCCESS) && makePersistent)
   {
      bool persistenceRes = inode->updateInodeOnDisk(entryInfo);
      if(!persistenceRes)
         retVal = FhgfsOpsErr_INTERNAL;
   }

   metaStore->releaseFile(entryInfo->getParentEntryID(), inode);

   return retVal;
}

FhgfsOpsErr MsgHelperStat::refreshDynAttribsSequential(FileInode* file, std::string entryID)
{
   const char* logContext = "Stat Helper (refresh chunk files S)";

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS; // will be set to node error, if any

   StripePattern* pattern = file->getStripePattern();
   const UInt16Vector* targetIDs = pattern->getStripeTargetIDs();
   TargetMapper* targetMapper = Program::getApp()->getTargetMapper();
   NodeStore* nodes = Program::getApp()->getStorageNodes();

   DynamicFileAttribsVec dynAttribsVec(targetIDs->size() );


   // send request to each node and receive the response message
   unsigned currentStripeNodeIndex = 0;
   for(UInt16VectorConstIter iter = targetIDs->begin();
      iter != targetIDs->end();
      iter++, currentStripeNodeIndex++)
   {
      uint16_t targetID = *iter;

      Node* node = nodes->referenceNodeByTargetID(targetID, targetMapper);
      if(!node)
      {
         LogContext(logContext).log(Log_WARNING,
            "Unable to resolve storage node targetID: " + StringTk::uintToStr(targetID) + "; "
            "entryID: " + entryID);

         retVal = FhgfsOpsErr_UNKNOWNNODE;
         continue;
      }

      GetChunkFileAttribsMsg getSizeMsg(entryID, targetID);

      // send request to node and receive response
      char* respBuf;
      NetMessage* respMsg;
      bool requestRes = MessagingTk::requestResponse(
         node, &getSizeMsg, NETMSGTYPE_GetChunkFileAttribsResp, &respBuf, &respMsg);

      if(!requestRes)
      { // communication error
         LogContext(logContext).log(2,
            "Communication with storage node failed: " + node->getTypedNodeID() + "; "
            "entryID: " + entryID);

         nodes->releaseNode(&node);
         retVal = FhgfsOpsErr_COMMUNICATION;
         continue;
      }

      // correct response type received
      GetChunkFileAttribsRespMsg* getSizeRespMsg = (GetChunkFileAttribsRespMsg*)respMsg;

      bool getSizeSuccess = (getSizeRespMsg->getResult() == 0);
      if(!getSizeSuccess)
      { // error: local file not unlinked
         LogContext(logContext).log(Log_WARNING,
            "Storage node was unable to get local filesize: " + node->getTypedNodeID() + "; "
            "entryID: " + entryID);

         nodes->releaseNode(&node);
         delete(respMsg);
         free(respBuf);
         retVal = FhgfsOpsErr_INTERNAL;
         continue;
      }

      // success: local file size refreshed
      //log.log(3, std::string("Got fresh filesize from node: ") + nodeID);

      DynamicFileAttribs currentDynAttribs(getSizeRespMsg->getStorageVersion(),
         getSizeRespMsg->getSize(), getSizeRespMsg->getModificationTimeSecs(),
         getSizeRespMsg->getLastAccessTimeSecs() );

      dynAttribsVec[currentStripeNodeIndex] = currentDynAttribs;

      nodes->releaseNode(&node);
      delete(respMsg);
      free(respBuf);
   }


   file->setDynAttribs(dynAttribsVec, (retVal != FhgfsOpsErr_SUCCESS) ); // the actual update

   if(retVal != FhgfsOpsErr_SUCCESS)
   {
      LogContext(logContext).log(2, "Problems occurred during file attribs refresh.");
   }

   return retVal;
}

FhgfsOpsErr MsgHelperStat::refreshDynAttribsParallel(FileInode* file, std::string entryID)
{
   const char* logContext = "Stat Helper (refresh chunk files)";

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS; // will be set to node error, if any

   App* app = Program::getApp();
   MultiWorkQueue* slaveQ = app->getCommSlaveQueue();
   StripePattern* pattern = file->getStripePattern();
   const UInt16Vector* targetIDs = pattern->getStripeTargetIDs();

   DynamicFileAttribsVec dynAttribsVec(targetIDs->size() );

   size_t numWorks = targetIDs->size();

   FhgfsOpsErrVec nodeResults(numWorks);
   SynchronizedCounter counter;

   for(size_t i=0; i < numWorks; i++)
   {
      Work* work = new GetChunkFileAttribsWork(entryID, (*targetIDs)[i],
         &(dynAttribsVec[i]), &(nodeResults[i]), &counter);

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

   file->setDynAttribs(dynAttribsVec, (retVal != FhgfsOpsErr_SUCCESS) ); // the actual update

   return retVal;
}
