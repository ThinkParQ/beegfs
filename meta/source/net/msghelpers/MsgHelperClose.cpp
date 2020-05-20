#include <common/net/message/session/opening/CloseChunkFileMsg.h>
#include <common/net/message/session/opening/CloseChunkFileRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/SessionTk.h>
#include <components/worker/CloseChunkFileWork.h>
#include <net/msghelpers/MsgHelperUnlink.h>
#include <program/Program.h>
#include <session/SessionStore.h>
#include <storage/MetaStore.h>
#include "MsgHelperClose.h"

#include <boost/lexical_cast.hpp>

/**
 * The wrapper for closeSessionFile() and closeChunkFile().
 *
 * @param maxUsedNodeIndex zero-based index, -1 means "none"
 * @param msgUserID only used for msg header info.
 * @param outUnlinkDisposalFile true if the hardlink count of the file was 0
 */
FhgfsOpsErr MsgHelperClose::closeFile(const NumNodeID sessionID, const std::string& fileHandleID,
   EntryInfo* entryInfo, int maxUsedNodeIndex, unsigned msgUserID, bool* outUnlinkDisposalFile,
   bool* outModificationEventsMissed, DynamicFileAttribsVec* dynAttribs,
   MirroredTimestamps* timestamps)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   unsigned accessFlags;
   unsigned numHardlinks;
   unsigned numInodeRefs;

   MetaFileHandle inode;

   *outUnlinkDisposalFile = false;


   FhgfsOpsErr sessionRes = closeSessionFile(sessionID, fileHandleID, entryInfo,
      &accessFlags, inode);
   if(unlikely(sessionRes != FhgfsOpsErr_SUCCESS) )
      return sessionRes;

   FhgfsOpsErr chunksRes = closeChunkFile(
      sessionID, fileHandleID, maxUsedNodeIndex, *inode, entryInfo, msgUserID, dynAttribs);

   if (timestamps)
   {
      StatData sd;

      inode->getStatData(sd);
      *timestamps = sd.getMirroredTimestamps();
   }

   if (outModificationEventsMissed)
      *outModificationEventsMissed = inode->getNumHardlinks() > 1;

   metaStore->closeFile(entryInfo, std::move(inode), accessFlags, &numHardlinks, &numInodeRefs);


   if (!numHardlinks && !numInodeRefs)
      *outUnlinkDisposalFile = true;

   return chunksRes;
}

/**
 * Close session in SessionStore.
 *
 * @param outCloseFile caller is responsible for calling MetaStore::closeFile() later if we
 * returned success
 */
FhgfsOpsErr MsgHelperClose::closeSessionFile(const NumNodeID sessionID,
   const std::string& fileHandleID, EntryInfo* entryInfo, unsigned* outAccessFlags,
   MetaFileHandle& outCloseInode)
{
   const char* logContext = "Close Helper (close session file)";

   FhgfsOpsErr closeRes = FhgfsOpsErr_INTERNAL;
   unsigned ownerFD = SessionTk::ownerFDFromHandleID(fileHandleID);

   outCloseInode = {};

   // find sessionFile
   SessionStore* sessions = entryInfo->getIsBuddyMirrored()
      ? Program::getApp()->getMirroredSessions()
      : Program::getApp()->getSessions();
   Session* session = sessions->referenceSession(sessionID, true);
   SessionFileStore* sessionFiles = session->getFiles();
   SessionFile* sessionFile = sessionFiles->referenceSession(ownerFD);

   if(!sessionFile)
   { // sessionFile not exists
      // note: nevertheless, we try to forward the close to the storage servers,
      // because the meta-server just might have been restarted (for whatever reason).
      // so we open the file here (if possible) and let the caller go on as if nothing was wrong...

      MetaStore* metaStore = Program::getApp()->getMetaStore();

      LogContext(logContext).log(Log_DEBUG, std::string("File not open ") +
         "(session: " + sessionID.str()               + "; "
         "handle: " + StringTk::uintToStr(ownerFD)    + "; "
         "parentID: " + entryInfo->getParentEntryID() + "; "
         "ID: " + entryInfo->getEntryID() + ")" );

      *outAccessFlags = OPENFILE_ACCESS_READWRITE;

      closeRes = metaStore->openFile(entryInfo, *outAccessFlags, outCloseInode);
   }
   else
   { // sessionFile exists

      // save access flags and file for later
      outCloseInode = sessionFile->releaseInode();
      *outAccessFlags = sessionFile->getAccessFlags();

      sessionFiles->releaseSession(sessionFile, entryInfo);

      if(!sessionFiles->removeSession(ownerFD) )
      { // removal failed
         LogContext(logContext).log(Log_WARNING, "Unable to remove file session "
            "(still in use, marked for async cleanup now). "
            "SessionID: " + sessionID.str() + "; "
            "FileHandle: " + std::string(fileHandleID) );
      }
      else
      { // file session removed => caller can close file
         closeRes = FhgfsOpsErr_SUCCESS;
      }

   }

   sessions->releaseSession(session);

   return closeRes;
}

/**
 * Close chunk files on storage servers.
 *
 * Note: This method is also called by the hbMgr during client sync.
 *
 * @param msgUserID only for msg header info.
 */
FhgfsOpsErr MsgHelperClose::closeChunkFile(const NumNodeID sessionID,
   const std::string& fileHandleID, int maxUsedNodeIndex, FileInode& inode, EntryInfo *entryInfo,
   unsigned msgUserID, DynamicFileAttribsVec* dynAttribs)
{
   if(maxUsedNodeIndex == -1)
      return FhgfsOpsErr_SUCCESS; // file contents were not accessed => nothing to do
   else
   if( (maxUsedNodeIndex > 0) ||
       (inode.getStripePattern()->getPatternType() == StripePatternType_BuddyMirror) )
      return closeChunkFileParallel(
         sessionID, fileHandleID, maxUsedNodeIndex, inode, entryInfo, msgUserID, dynAttribs);
   else
      return closeChunkFileSequential(
         sessionID, fileHandleID, maxUsedNodeIndex, inode, entryInfo, msgUserID, dynAttribs);
}

/**
 * Note: This method does not work for mirrored files; use closeChunkFileParallel() for those.
 *
 * @param maxUsedNodeIndex (zero-based position in nodeID vector)
 * @param msgUserID only for msg header info.
 */
FhgfsOpsErr MsgHelperClose::closeChunkFileSequential(const NumNodeID sessionID,
   const std::string& fileHandleID, int maxUsedNodeIndex, FileInode& inode, EntryInfo *entryInfo,
   unsigned msgUserID, DynamicFileAttribsVec* dynAttribs)
{
   const char* logContext = "Close Helper (close chunk files S)";

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   TargetMapper* targetMapper = Program::getApp()->getTargetMapper();
   TargetStateStore* targetStates = Program::getApp()->getTargetStateStore();
   NodeStore* nodes = Program::getApp()->getStorageNodes();
   StripePattern* pattern = inode.getStripePattern();
   PathInfo pathInfo;
   const UInt16Vector* targetIDs = pattern->getStripeTargetIDs();

   DynamicFileAttribsVec dynAttribsVec(targetIDs->size() );

   inode.getPathInfo(&pathInfo);

   // send request to each node and receive the response message
   int currentTargetIndex = 0;
   for(UInt16VectorConstIter iter = targetIDs->begin();
      (currentTargetIndex <= maxUsedNodeIndex) && (iter != targetIDs->end() );
      iter++, currentTargetIndex++)
   {
      uint16_t targetID = *iter;

      CloseChunkFileMsg closeMsg(sessionID, fileHandleID, targetID, &pathInfo);

      closeMsg.setMsgHeaderUserID(msgUserID);

      RequestResponseArgs rrArgs(NULL, &closeMsg, NETMSGTYPE_CloseChunkFileResp);
      RequestResponseTarget rrTarget(targetID, targetMapper, nodes);

      rrTarget.setTargetStates(targetStates);

      // send request to node and receive response
      FhgfsOpsErr requestRes = MessagingTk::requestResponseTarget(&rrTarget, &rrArgs);

      if(requestRes != FhgfsOpsErr_SUCCESS)
      { // communication error
         LogContext(logContext).log(Log_WARNING,
            "Communication with storage target failed: " + StringTk::uintToStr(targetID) + "; "
            "FileHandle: " + fileHandleID + "; "
            "Error: " + boost::lexical_cast<std::string>(requestRes));

         if(retVal == FhgfsOpsErr_SUCCESS)
            retVal = requestRes;

         continue;
      }

      // correct response type received
      CloseChunkFileRespMsg* closeRespMsg = (CloseChunkFileRespMsg*)rrArgs.outRespMsg.get();

      FhgfsOpsErr closeRemoteRes = closeRespMsg->getResult();

      // set current dynamic attribs (even if result not success, because then storageVersion==0)
      DynamicFileAttribs currentDynAttribs(closeRespMsg->getStorageVersion(),
         closeRespMsg->getFileSize(), closeRespMsg->getAllocedBlocks(),
         closeRespMsg->getModificationTimeSecs(), closeRespMsg->getLastAccessTimeSecs() );

      dynAttribsVec[currentTargetIndex] = currentDynAttribs;

      if(unlikely(closeRemoteRes != FhgfsOpsErr_SUCCESS) )
      { // error: chunk file close problem
         int logLevel = Log_WARNING;

         if(closeRemoteRes == FhgfsOpsErr_INUSE)
            logLevel = Log_DEBUG; // happens on ctrl+c, so don't irritate user with these log msgs

         LogContext(logContext).log(logLevel,
            "Storage target was unable to close chunk file: " + StringTk::uintToStr(targetID) + "; "
            "Error: " + boost::lexical_cast<std::string>(closeRemoteRes) + "; "
            "Session: " + sessionID.str() + "; "
            "FileHandle: " + fileHandleID);

         if(closeRemoteRes == FhgfsOpsErr_INUSE)
            continue; // don't escalate this error to client (happens on ctrl+c)

         retVal = closeRemoteRes;
         continue;
      }

      // success: chunk file closed
      LOG_DEBUG(logContext, Log_DEBUG,
         "Storage target closed chunk file: " + StringTk::uintToStr(targetID) + "; "
         "FileHandle: " + fileHandleID);
   }

   inode.setDynAttribs(dynAttribsVec); // the actual update
   if (dynAttribs)
      dynAttribs->swap(dynAttribsVec);

   if(unlikely(retVal != FhgfsOpsErr_SUCCESS) )
      LogContext(logContext).log(Log_WARNING,
         "Problems occurred during close of chunk files. "
         "FileHandle: " + fileHandleID);

   return retVal;
}


/**
 * @param maxUsedNodeIndex (zero-based position in nodeID vector)
 * @param msgUserID only for msg header info.
 */
FhgfsOpsErr MsgHelperClose::closeChunkFileParallel(const NumNodeID sessionID,
   const std::string& fileHandleID, int maxUsedNodeIndex, FileInode& inode, EntryInfo* entryInfo,
   unsigned msgUserID, DynamicFileAttribsVec* dynAttribs)
{
   const char* logContext = "Close Helper (close chunk files)";

   App* app = Program::getApp();
   MultiWorkQueue* slaveQ = app->getCommSlaveQueue();
   StripePattern* pattern = inode.getStripePattern();
   const UInt16Vector* targetIDs = pattern->getStripeTargetIDs();
   PathInfo pathInfo;

   size_t numTargetWorksHint = (maxUsedNodeIndex < 0) ? 0 : (maxUsedNodeIndex+1);
   size_t numTargetWorks = BEEGFS_MIN(numTargetWorksHint, targetIDs->size() );
   
   DynamicFileAttribsVec dynAttribsVec(targetIDs->size() );

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;
   FhgfsOpsErrVec nodeResults(numTargetWorks);
   SynchronizedCounter counter;

   inode.getPathInfo(&pathInfo);

   // generate work for storage targets...
   for(size_t i=0; i < numTargetWorks; i++)
   {
      CloseChunkFileWork* work = new CloseChunkFileWork(sessionID, fileHandleID, pattern,
         (*targetIDs)[i], &pathInfo, &(dynAttribsVec[i]), &(nodeResults[i]), &counter);

      work->setMsgUserID(msgUserID);

      slaveQ->addDirectWork(work);
   }

   // wait for work completion...

   counter.waitForCount(numTargetWorks);

   // check target results...

   for(size_t i=0; i < numTargetWorks; i++)
   {
      if(unlikely(nodeResults[i] != FhgfsOpsErr_SUCCESS) )
      {
         if(nodeResults[i] == FhgfsOpsErr_INUSE)
            continue; // don't escalate this error to client (happens on ctrl+c)

         LogContext(logContext).log(Log_WARNING,
            "Problems occurred during release of storage server file handles. "
            "FileHandle: " + std::string(fileHandleID) );

         retVal = nodeResults[i];
         goto apply_dyn_attribs;
      }
   }


apply_dyn_attribs:

   inode.setDynAttribs(dynAttribsVec); // the actual update
   if (dynAttribs)
      dynAttribs->swap(dynAttribsVec);

   return retVal;
}


/**
 * Unlink file in META_DISPOSALDIR_ID_STR/
 *
 * @param msgUserID only for msg header info.
 */
FhgfsOpsErr MsgHelperClose::unlinkDisposableFile(const std::string& fileID, unsigned msgUserID,
      bool isBuddyMirrored)
{
    if (isBuddyMirrored && 0 < Program::getApp()->getConfig()->getTuneDisposalGCPeriod())
        return FhgfsOpsErr_SUCCESS;

   // Note: This attempt to unlink directly is inefficient if the file is marked as disposable
   //    and is still busy (but we assume that this rarely happens)

   DirInode* dir = Program::getApp()->getMetaStore()->referenceDir(
         isBuddyMirrored ? META_MIRRORDISPOSALDIR_ID_STR : META_DISPOSALDIR_ID_STR,
         isBuddyMirrored, true);
   if (!dir)
      return FhgfsOpsErr_INTERNAL;

   FhgfsOpsErr disposeRes = MsgHelperUnlink::unlinkFile(*dir, fileID, msgUserID);
   if (disposeRes == FhgfsOpsErr_PATHNOTEXISTS)
      disposeRes = FhgfsOpsErr_SUCCESS; // file not marked for disposal => not an error

   Program::getApp()->getMetaStore()->releaseDir(dir->getID());

   return disposeRes;
}
