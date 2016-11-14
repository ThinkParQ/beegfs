#include <common/net/message/storage/TruncLocalFileMsg.h>
#include <common/net/message/storage/TruncLocalFileRespMsg.h>
#include <components/worker/TruncLocalFileWork.h>
#include <program/Program.h>
#include "MsgHelperTrunc.h"


/**
 * Note: Will also update persistent metadata on disk.
 */
FhgfsOpsErr MsgHelperTrunc::truncFile(EntryInfo* entryInfo, int64_t filesize)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   FileInode* truncFile = metaStore->referenceFile(entryInfo);
   if(!truncFile)
      return FhgfsOpsErr_PATHNOTEXISTS;
   
   FhgfsOpsErr localRes = truncChunkFile(truncFile, entryInfo, filesize);
   
   truncFile->updateInodeOnDisk(entryInfo);

   metaStore->releaseFile(entryInfo->getParentEntryID(), truncFile);
   
   return localRes;
}

/**
 * Note: This also updates dynamic file attribs.
 * Note: Call this directly (instead of truncFile() ) if you already got the file handle.
 * Note: Will NOT update persistent metadata on disk.
 */
FhgfsOpsErr MsgHelperTrunc::truncChunkFile(FileInode* file, EntryInfo* entryInfo, int64_t filesize)
{
   StripePattern* pattern = file->getStripePattern();

   if( (pattern->getStripeTargetIDs()->size() > 1) || pattern->getMirrorTargetIDs() )
      return truncChunkFileParallel(file, entryInfo, filesize);
   else
      return truncChunkFileSequential(file, entryInfo, filesize);
}

/**
 * Note: This method does not work for mirrored files; use truncChunkFileParallel() for those.
 * Note: This also updates dynamic file attribs.
 */
FhgfsOpsErr MsgHelperTrunc::truncChunkFileSequential(FileInode* file, EntryInfo* entryInfo,
   int64_t filesize)
{
   const char* logContext = "Trunc chunk file helper S";
   
   StripePattern* pattern = file->getStripePattern();
   const UInt16Vector* targetIDs = pattern->getStripeTargetIDs();
   TargetMapper* targetMapper = Program::getApp()->getTargetMapper();
   NodeStore* nodes = Program::getApp()->getStorageNodes();
   DynamicFileAttribsVec dynAttribsVec(targetIDs->size() );
   
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   // send request to each node and receive the response message
   unsigned currentNodeIndex = 0;
   for(UInt16VectorConstIter iter = targetIDs->begin();
      iter != targetIDs->end();
      iter++, currentNodeIndex++)
   {
      uint16_t targetID = *iter;

      Node* node = nodes->referenceNodeByTargetID(targetID, targetMapper);
      if(!node)
      {
         LogContext(logContext).log(Log_WARNING,
            "Unable to resolve storage node targetID: " + StringTk::uintToStr(targetID) + "; "
            "fileID: " + file->getEntryID() );

         if(retVal == FhgfsOpsErr_SUCCESS)
            retVal = FhgfsOpsErr_INTERNAL;

         continue;
      }

      std::string entryID = entryInfo->getEntryID();
      int64_t truncPos = getNodeLocalTruncPos(filesize, *pattern, currentNodeIndex);
      TruncLocalFileMsg truncMsg(truncPos, entryID, targetID);

      // send request to node and receive response
      char* respBuf;
      NetMessage* respMsg;
      bool requestRes = MessagingTk::requestResponse(
         node, &truncMsg, NETMSGTYPE_TruncLocalFileResp, &respBuf, &respMsg);

      if(!requestRes)
      { // communication error
         LogContext(logContext).log(Log_WARNING,
            "Communication with node failed: " + node->getTypedNodeID() + "; "
            "fileID: " + file->getEntryID() );

         if(retVal == FhgfsOpsErr_SUCCESS)
            retVal = FhgfsOpsErr_COMMUNICATION;

         nodes->releaseNode(&node);
         continue;
      }
      
      // correct response type received
      TruncLocalFileRespMsg* truncRespMsg = (TruncLocalFileRespMsg*)respMsg;
      
      // set current dynamic attribs (even if result not success, because then storageVersion==0)
      DynamicFileAttribs currentDynAttribs(truncRespMsg->getStorageVersion(),
         truncRespMsg->getFileSize(), truncRespMsg->getModificationTimeSecs(),
         truncRespMsg->getLastAccessTimeSecs() );

      dynAttribsVec[currentNodeIndex] = currentDynAttribs;

      FhgfsOpsErr chunkTruncRes = (FhgfsOpsErr) truncRespMsg->getResult();
      if(chunkTruncRes != FhgfsOpsErr_SUCCESS)
      { // error: local file not truncated
         LogContext(logContext).log(Log_WARNING,
            "Storage node was unable to truncate chunk file: " + node->getTypedNodeID() +
            std::string("fileID: ") + file->getEntryID() +
            " Error: " + FhgfsOpsErrTk::toErrString(chunkTruncRes) );
         
         if(retVal == FhgfsOpsErr_SUCCESS)
            retVal = (FhgfsOpsErr)truncRespMsg->getResult();

         nodes->releaseNode(&node);
         delete(respMsg);
         free(respBuf);
         
         continue;
      }
      
      // success: local file unlinked
      LOG_DEBUG(logContext, Log_DEBUG,
         "Storage node truncated chunk file: " + node->getTypedNodeID() + "; "
         "fileID: " + file->getEntryID() );
      
      nodes->releaseNode(&node);
      delete(respMsg);
      free(respBuf);
   }


   file->setDynAttribs(dynAttribsVec, (retVal != FhgfsOpsErr_SUCCESS) ); // the actual update

   if(retVal != FhgfsOpsErr_SUCCESS)
   {
      LogContext(logContext).log(Log_WARNING,
         "Problems occurred during truncation of storage server chunk files. "
         "fileID: " + file->getEntryID() );
   }

   return retVal;
}

/**
 * Note: This also updates dynamic file attribs.
 */
FhgfsOpsErr MsgHelperTrunc::truncChunkFileParallel(FileInode* file, EntryInfo* entryInfo,
   int64_t filesize)
{
   const char* logContext = "Trunc chunk file helper";

   App* app = Program::getApp();
   MultiWorkQueue* slaveQ = app->getCommSlaveQueue();
   StripePattern* pattern = file->getStripePattern();
   const UInt16Vector* targetIDs = pattern->getStripeTargetIDs();
   const UInt16Vector* mirrorTargetIDs = pattern->getMirrorTargetIDs(); // NULL if not raid10

   size_t numTargetWorks = targetIDs->size();
   size_t numMirrorWorks = mirrorTargetIDs ? numTargetWorks : 0;
   size_t numWorksTotal = numTargetWorks + numMirrorWorks;

   DynamicFileAttribsVec dynAttribsVec(targetIDs->size() ); // (only targets, not mirrors)

   FhgfsOpsErrVec nodeResults(numWorksTotal); // 1st half for targets, 2nd half for mirrors
   SynchronizedCounter counter;

   // generate work for storage targets...

   for(size_t i=0; i < numTargetWorks; i++)
   {
      int64_t localTruncPos = getNodeLocalTruncPos(filesize, *pattern, i);

      TruncLocalFileWork* work = new TruncLocalFileWork(file->getEntryID(), localTruncPos,
         (*targetIDs)[i], &(dynAttribsVec[i]), &(nodeResults[i]), &counter);

      slaveQ->addDirectWork(work);
   }

   // generate work for mirror targets...

   for(size_t i=0; i < numMirrorWorks; i++)
   {
      int64_t localTruncPos = getNodeLocalTruncPos(filesize, *pattern, i);

      TruncLocalFileWork* work = new TruncLocalFileWork(file->getEntryID(), localTruncPos,
         (*mirrorTargetIDs)[i], NULL, &(nodeResults[numTargetWorks + i] ), &counter);

      work->setMirroredFromTargetID( (*targetIDs)[i] );

      slaveQ->addDirectWork(work);
   }

   // wait for work completion...

   counter.waitForCount(numWorksTotal);

   // check target results...

   bool totalSuccess = true;

   for(size_t i=0; i < numTargetWorks; i++)
   {
      if(unlikely(nodeResults[i] != FhgfsOpsErr_SUCCESS) )
      {
         LogContext(logContext).log(Log_WARNING,
            "Problems occurred during truncation of storage server chunk files. "
            "File: " + file->getEntryID() );

         totalSuccess = false;
         goto apply_dyn_attribs;
      }
   }

   // check mirror results...

   for(size_t i=0; i < numMirrorWorks; i++)
   {
      if(unlikely(nodeResults[numTargetWorks + i] != FhgfsOpsErr_SUCCESS) )
      {
         LogContext(logContext).log(Log_WARNING,
            "Problems occurred during truncation of storage server (mirror) chunk files. "
            "File: " + file->getEntryID() );

         totalSuccess = false;
         goto apply_dyn_attribs;
      }
   }


apply_dyn_attribs:

   file->setDynAttribs(dynAttribsVec, !totalSuccess); // the actual update

   // note: we only return success or interal error because more detailed results could be
      // confusing in the client log
   return totalSuccess ? FhgfsOpsErr_SUCCESS : FhgfsOpsErr_INTERNAL;
}

/**
 * Note: Makes only sense to call this before opening the file (because afterwards the filesize
 * will be unknown).
 *
 * @param filesize desired trunc filesize
 * @return true if current filesize is unknown or not equal to given filesize
 */
bool MsgHelperTrunc::isTruncChunkRequired(EntryInfo* entryInfo, int64_t filesize)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   StatData statData;

   FhgfsOpsErr statRes = metaStore->stat(entryInfo, true, statData);

   if( (statRes == FhgfsOpsErr_PATHNOTEXISTS) && (!filesize) )
      return false; // does not exist => no zero-trunc necessary

   if( (statRes == FhgfsOpsErr_SUCCESS) && (filesize == statData.getFileSize() ) )
      return false; // exists and already has desired size

   return true;
}


int64_t MsgHelperTrunc::getNodeLocalOffset(int64_t pos, int64_t chunkSize, size_t numNodes,
   size_t stripeNodeIndex)
{
   // (note: we can use "& ... -1" here instead if "%", because chunkSize is a po wer of two
   int64_t posModChunkSize = pos & (chunkSize - 1);
   int64_t chunkStart = pos - posModChunkSize - (stripeNodeIndex*chunkSize);
   
   return ( (chunkStart / numNodes) + posModChunkSize);
}

int64_t MsgHelperTrunc::getNodeLocalTruncPos(int64_t pos, StripePattern& pattern,
   size_t stripeNodeIndex)
{
   int64_t truncPos;
   size_t numTargets = pattern.getStripeTargetIDs()->size();
   
   size_t mainNodeIndex = pattern.getStripeTargetIndex(pos);
   int64_t mainNodeLocalOffset = getNodeLocalOffset(
      pos, pattern.getChunkSize(), numTargets, mainNodeIndex);
      
   if(stripeNodeIndex < mainNodeIndex)
      truncPos = pattern.getNextChunkStart(mainNodeLocalOffset);
   else
   if(stripeNodeIndex == mainNodeIndex)
      truncPos = mainNodeLocalOffset;
   else
      truncPos = pattern.getChunkStart(mainNodeLocalOffset);
      
   truncPos = FHGFS_MAX(truncPos, 0);
   
   return truncPos;
}
