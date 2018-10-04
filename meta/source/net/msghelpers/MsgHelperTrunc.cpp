#include <common/net/message/storage/TruncLocalFileMsg.h>
#include <common/net/message/storage/TruncLocalFileRespMsg.h>
#include <components/worker/TruncChunkFileWork.h>
#include <program/Program.h>
#include "MsgHelperTrunc.h"

#include <boost/lexical_cast.hpp>


/**
 * Note: Will also update persistent metadata on disk.
 *
 * @param msgUserID will only be used in msg header info.
 */
FhgfsOpsErr MsgHelperTrunc::truncFile(EntryInfo* entryInfo, int64_t filesize, bool useQuota,
   unsigned msgUserID, DynamicFileAttribsVec& dynAttribs)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   MetaFileHandle inode = metaStore->referenceFile(entryInfo);
   if(!inode)
      return FhgfsOpsErr_PATHNOTEXISTS;

   FhgfsOpsErr localRes = truncChunkFile(*inode, entryInfo, filesize, useQuota, msgUserID,
         dynAttribs);

   inode->updateInodeOnDisk(entryInfo);

   metaStore->releaseFile(entryInfo->getParentEntryID(), inode);

   return localRes;
}

/**
 * Note: This also updates dynamic file attribs.
 * Note: Call this directly (instead of truncFile() ) if you already got the file handle.
 * Note: Will NOT update persistent metadata on disk.
 *
 * @param msgUserID only used for msg header info.
 */
FhgfsOpsErr MsgHelperTrunc::truncChunkFile(FileInode& inode, EntryInfo* entryInfo,
   int64_t filesize, bool useQuota, unsigned userIDHint, DynamicFileAttribsVec& dynAttribs)
{
   StripePattern* pattern = inode.getStripePattern();

   if( (pattern->getStripeTargetIDs()->size() > 1) ||
       (pattern->getPatternType() == StripePatternType_BuddyMirror) )
      return truncChunkFileParallel(inode, entryInfo, filesize, useQuota, userIDHint, dynAttribs);
   else
      return truncChunkFileSequential(inode, entryInfo, filesize, useQuota, userIDHint, dynAttribs);
}

/**
 * Note: This method does not work for mirrored files; use truncChunkFileParallel() for those.
 * Note: This also updates dynamic file attribs.
 *
 * @param msgUserID only used for msg header info.
 */
FhgfsOpsErr MsgHelperTrunc::truncChunkFileSequential(FileInode& inode, EntryInfo* entryInfo,
   int64_t filesize, bool useQuota, unsigned msgUserID, DynamicFileAttribsVec& dynAttribs)
{
   const char* logContext = "Trunc chunk file helper S";
   
   StripePattern* pattern = inode.getStripePattern();
   const UInt16Vector* targetIDs = pattern->getStripeTargetIDs();
   TargetMapper* targetMapper = Program::getApp()->getTargetMapper();
   TargetStateStore* targetStates = Program::getApp()->getTargetStateStore();
   NodeStore* nodes = Program::getApp()->getStorageNodes();

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   PathInfo pathInfo;
   inode.getPathInfo(&pathInfo);

   dynAttribs.resize(targetIDs->size());

   // send request to each node and receive the response message
   unsigned currentNodeIndex = 0;
   for(UInt16VectorConstIter iter = targetIDs->begin();
      iter != targetIDs->end();
      iter++, currentNodeIndex++)
   {
      uint16_t targetID = *iter;

      std::string entryID = entryInfo->getEntryID();
      int64_t truncPos = getNodeLocalTruncPos(filesize, *pattern, currentNodeIndex);
      TruncLocalFileMsg truncMsg(truncPos, entryID, targetID, &pathInfo);

      if (useQuota)
         truncMsg.setUserdataForQuota(inode.getUserID(), inode.getGroupID());

      truncMsg.setMsgHeaderUserID(msgUserID);

      RequestResponseArgs rrArgs(NULL, &truncMsg, NETMSGTYPE_TruncLocalFileResp);
      RequestResponseTarget rrTarget(targetID, targetMapper, nodes);

      rrTarget.setTargetStates(targetStates);

      // send request to node and receive response
      FhgfsOpsErr requestRes = MessagingTk::requestResponseTarget(&rrTarget, &rrArgs);

      if(requestRes != FhgfsOpsErr_SUCCESS)
      { // communication error
         LogContext(logContext).log(Log_WARNING,
            "Communication with storage target failed: " + StringTk::uintToStr(targetID) + "; "
            "fileID: " + inode.getEntryID());

         if(retVal == FhgfsOpsErr_SUCCESS)
            retVal = requestRes;

         continue;
      }
      
      // correct response type received
      TruncLocalFileRespMsg* truncRespMsg = (TruncLocalFileRespMsg*)rrArgs.outRespMsg.get();
      
      // set current dynamic attribs (even if result not success, because then storageVersion==0)
      DynamicFileAttribs currentDynAttribs(truncRespMsg->getStorageVersion(),
         truncRespMsg->getFileSize(), truncRespMsg->getAllocedBlocks(),
         truncRespMsg->getModificationTimeSecs(), truncRespMsg->getLastAccessTimeSecs() );

      dynAttribs[currentNodeIndex] = currentDynAttribs;

      FhgfsOpsErr chunkTruncRes = truncRespMsg->getResult();
      if(chunkTruncRes != FhgfsOpsErr_SUCCESS)
      { // error: chunk file not truncated
         LogContext(logContext).log(Log_WARNING,
            "Storage target failed to truncate chunk file: " + StringTk::uintToStr(targetID) + "; "
            "fileID: " + inode.getEntryID() + "; "
            "Error: " + boost::lexical_cast<std::string>(chunkTruncRes));

         if(retVal == FhgfsOpsErr_SUCCESS)
            retVal = truncRespMsg->getResult();
         
         continue;
      }
      
      // success: local inode unlinked
      LOG_DEBUG(logContext, Log_DEBUG,
         "Storage target truncated chunk file: " + StringTk::uintToStr(targetID) + "; "
         "fileID: " + inode.getEntryID());
   }


   inode.setDynAttribs(dynAttribs); // the actual update

   if(unlikely( (retVal != FhgfsOpsErr_SUCCESS) && (retVal != FhgfsOpsErr_TOOBIG) ) )
   {
      LogContext(logContext).log(Log_WARNING,
         "Problems occurred during truncation of chunk files. "
         "fileID: " + inode.getEntryID());
   }

   return retVal;
}

/**
 * Note: This also updates dynamic file attribs.
 *
 * @param msgUserID only used for msg header info.
 */
FhgfsOpsErr MsgHelperTrunc::truncChunkFileParallel(FileInode& inode, EntryInfo* entryInfo,
   int64_t filesize, bool useQuota, unsigned msgUserID, DynamicFileAttribsVec& dynAttribs)
{
   const char* logContext = "Trunc chunk file helper";

   App* app = Program::getApp();
   MultiWorkQueue* slaveQ = app->getCommSlaveQueue();
   StripePattern* pattern = inode.getStripePattern();
   const UInt16Vector* targetIDs = pattern->getStripeTargetIDs();

   size_t numTargetWorks = targetIDs->size();

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;
   FhgfsOpsErrVec nodeResults(numTargetWorks);
   SynchronizedCounter counter;

   // generate work for storage targets...
   PathInfo pathInfo;
   inode.getPathInfo(&pathInfo);

   dynAttribs.resize(numTargetWorks);

   for(size_t i=0; i < numTargetWorks; i++)
   {
      int64_t localTruncPos = getNodeLocalTruncPos(filesize, *pattern, i);

      TruncChunkFileWork* work = new TruncChunkFileWork(inode.getEntryID(), localTruncPos,
         pattern, (*targetIDs)[i], &pathInfo, &dynAttribs[i], &(nodeResults[i]), &counter);

      if (useQuota)
         work->setUserdataForQuota(inode.getUserID(), inode.getGroupID());

      work->setMsgUserID(msgUserID);

      slaveQ->addDirectWork(work);
   }

   // wait for work completion...

   counter.waitForCount(numTargetWorks);

   // check target results...

   for(size_t i=0; i < numTargetWorks; i++)
   {
      FhgfsOpsErr nodeResult = nodeResults[i];
      if(unlikely(nodeResult != FhgfsOpsErr_SUCCESS) )
      {
         LogContext(logContext).log(Log_WARNING,
            "Problems occurred during truncation of storage server chunk files. "
            "File: " + inode.getEntryID());

         retVal = nodeResult;
         break;
      }
   }

   inode.setDynAttribs(dynAttribs); // the actual update

   return retVal;
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
      
   truncPos = BEEGFS_MAX(truncPos, 0);
   
   return truncPos;
}
