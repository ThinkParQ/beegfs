#include <common/net/message/session/opening/CloseChunkFileMsg.h>
#include <common/net/message/session/opening/CloseChunkFileRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/SessionTk.h>
#include <components/worker/CloseChunkFileWork.h>
#include <net/msghelpers/MsgHelperUnlink.h>
#include <program/Program.h>
#include <storage/MetaStore.h>
#include "MsgHelperClose.h"

/**
 * The wrapper for closeSessionFile() and closeChunkFile().
 *
 * @param maxUsedNodeIndex zero-based index, -1 means "none"
 * @param outFileWasUnlinked true if the hardlink count of the file was 0
 */
FhgfsOpsErr MsgHelperClose::closeFile(std::string sessionID, std::string fileHandleID,
   EntryInfo* entryInfo, int maxUsedNodeIndex, std::string* outFileID, bool* unlinkDisposalFile)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   unsigned accessFlags;
   unsigned numHardlinks;
   unsigned numInodeRefs;

   FileInode* inode;

   FhgfsOpsErr sessionRes = closeSessionFile(sessionID, fileHandleID, entryInfo,
      &accessFlags, &inode);
   if(sessionRes != FhgfsOpsErr_SUCCESS)
      return sessionRes;

   FhgfsOpsErr chunksRes = closeChunkFile(sessionID, fileHandleID, maxUsedNodeIndex, inode);

   *outFileID = inode->getEntryID();

   metaStore->closeFile(entryInfo, inode, accessFlags, &numHardlinks, &numInodeRefs);

   if (!numHardlinks && !numInodeRefs)
      *unlinkDisposalFile = true;

   return chunksRes;
}

/**
 * @param outCloseFile caller is responsible for calling MetaStore::closeFile() later if we
 * returned success
 */
FhgfsOpsErr MsgHelperClose::closeSessionFile(std::string sessionID, std::string fileHandleID,
   EntryInfo* entryInfo, unsigned* outAccessFlags, FileInode** outCloseInode)
{
   const char* logContext = "Close Helper (close session file)";

   FhgfsOpsErr closeRes = FhgfsOpsErr_INTERNAL;
   unsigned ownerFD = SessionTk::ownerFDFromHandleID(fileHandleID);

   *outCloseInode = NULL;

   // find sessionFile
   SessionStore* sessions = Program::getApp()->getSessions();
   Session* session = sessions->referenceSession(sessionID, true);
   SessionFileStore* sessionFiles = session->getFiles();
   SessionFile* sessionFile = sessionFiles->referenceSession(ownerFD);

   if(!sessionFile)
   { // sessionFile not exists
      // note: nevertheless, we try to forward the close to the storage servers,
      // because the meta-server just might have been restarted (for whatever reason).
      // so we open the file here (if possible) and let the caller go on as if nothing was wrong...

      MetaStore* metaStore = Program::getApp()->getMetaStore();

      LogContext(logContext).log(2, std::string("File not open ") +
         "(session: " + sessionID                     + "; "
         "handle: " + StringTk::uintToStr(ownerFD)    + "; "
         "parentID: " + entryInfo->getParentEntryID() + "; "
         "ID: " + entryInfo->getEntryID() + ")" );

      *outAccessFlags = OPENFILE_ACCESS_READWRITE; // (actually doesn't matter)

      closeRes = metaStore->openFile(entryInfo, *outAccessFlags, outCloseInode);
   }
   else
   { // sessionFile exists

      // save access flags and file for later
      *outCloseInode = sessionFile->getInode();
      *outAccessFlags = sessionFile->getAccessFlags();

      sessionFiles->releaseSession(sessionFile, entryInfo);

      if(!sessionFiles->removeSession(ownerFD) )
      { // removal failed
         LogContext(logContext).log(2, "Unable to remove file session (still in use, marked for "
            "async cleanup now). " "SessionID: " + std::string(sessionID) + "; "
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
 * Note: This method is also called by the hbMgr during client sync.
 */
FhgfsOpsErr MsgHelperClose::closeChunkFile(std::string sessionID, std::string fileHandleID,
   int maxUsedNodeIndex, FileInode* inode)
{
   if(maxUsedNodeIndex == -1)
      return FhgfsOpsErr_SUCCESS; // file contents were not accessed => nothing to do
   else
   if( (maxUsedNodeIndex > 0) || inode->getStripePattern()->getMirrorTargetIDs() )
      return closeChunkFileParallel(sessionID, fileHandleID, maxUsedNodeIndex, inode);
   else
      return closeChunkFileSequential(sessionID, fileHandleID, maxUsedNodeIndex, inode);
}

/**
 * Note: This method does not work for mirrored files; use closeChunkFileParallel() for those.
 *
 * @param maxUsedNodeIndex (zero-based position in nodeID vector)
 */
FhgfsOpsErr MsgHelperClose::closeChunkFileSequential(std::string sessionID,
   std::string fileHandleID, int maxUsedNodeIndex, FileInode* inode)
{
   const char* logContext = "Close Helper (close chunk files S)";

   bool totalSuccess = true;

   TargetMapper* targetMapper = Program::getApp()->getTargetMapper();
   NodeStore* nodes = Program::getApp()->getStorageNodes();
   StripePattern* pattern = inode->getStripePattern();
   const UInt16Vector* targetIDs = pattern->getStripeTargetIDs();

   DynamicFileAttribsVec dynAttribsVec(targetIDs->size() );

   // send request to each node and receive the response message
   int currentTargetIndex = 0;
   for(UInt16VectorConstIter iter = targetIDs->begin();
      (currentTargetIndex <= maxUsedNodeIndex) && (iter != targetIDs->end() );
      iter++, currentTargetIndex++)
   {
      uint16_t targetID = *iter;

      Node* node = nodes->referenceNodeByTargetID(targetID, targetMapper);
      if(!node)
      {
         LogContext(logContext).log(Log_WARNING,
            "Unable to resolve storage node targetID: " + StringTk::uintToStr(targetID) + "; "
            "FileHandleID: " + fileHandleID);

         continue;
      }

      CloseChunkFileMsg closeMsg(sessionID, fileHandleID, targetID);

      // send request to node and receive response
      char* respBuf;
      NetMessage* respMsg;
      bool requestRes = MessagingTk::requestResponse(
         node, &closeMsg, NETMSGTYPE_CloseChunkFileResp, &respBuf, &respMsg);

      if(!requestRes)
      { // communication error
         LogContext(logContext).log(Log_WARNING,
            "Communication with storage node failed: " + node->getTypedNodeID() + "; "
            "FileHandle: " + fileHandleID);

         nodes->releaseNode(&node);
         totalSuccess = false;
         continue;
      }

      // correct response type received
      CloseChunkFileRespMsg* closeRespMsg = (CloseChunkFileRespMsg*)respMsg;

      FhgfsOpsErr closeRemoteRes = closeRespMsg->getResult();

      // set current dynamic attribs (even if result not success, because then storageVersion==0)
      DynamicFileAttribs currentDynAttribs(closeRespMsg->getStorageVersion(),
         closeRespMsg->getFileSize(), closeRespMsg->getModificationTimeSecs(),
         closeRespMsg->getLastAccessTimeSecs() );

      dynAttribsVec[currentTargetIndex] = currentDynAttribs;

      if(unlikely(closeRemoteRes != FhgfsOpsErr_SUCCESS) )
      { // error: chunk file close problem
         LogContext(logContext).log(Log_WARNING,
            "Storage node was unable to close chunk file: " + node->getTypedNodeID() + "; "
            "Error: " + FhgfsOpsErrTk::toErrString(closeRemoteRes) + "; "
            "Session: " + sessionID + "; "
            "FileHandle: " + fileHandleID);

         nodes->releaseNode(&node);
         delete(respMsg);
         free(respBuf);

         if(closeRemoteRes == FhgfsOpsErr_INUSE)
            continue; // don't escalate this error to client (happens on ctrl+c)

         totalSuccess = false;
         continue;
      }

      // success: chunk file closed
      LOG_DEBUG(logContext, Log_DEBUG,
         "Storage node closed local file: " + node->getTypedNodeID() + "; "
         "FileHandle: " + fileHandleID);

      nodes->releaseNode(&node);
      delete(respMsg);
      free(respBuf);
   }

   inode->setDynAttribs(dynAttribsVec, !totalSuccess); // the actual update

   if(!totalSuccess)
   {
      LogContext(logContext).log(Log_WARNING,
         "Problems occurred during release of storage server file handles. "
         "FileHandle: " + fileHandleID);

      return FhgfsOpsErr_INTERNAL;
   }

   return FhgfsOpsErr_SUCCESS;
}


/**
 * @param maxUsedNodeIndex (zero-based position in nodeID vector)
 */
FhgfsOpsErr MsgHelperClose::closeChunkFileParallel(std::string sessionID, std::string fileHandleID,
   int maxUsedNodeIndex, FileInode* file)
{
   const char* logContext = "Close Helper (close chunk files)";

   App* app = Program::getApp();
   MultiWorkQueue* slaveQ = app->getCommSlaveQueue();
   StripePattern* pattern = file->getStripePattern();
   const UInt16Vector* targetIDs = pattern->getStripeTargetIDs();
   const UInt16Vector* mirrorTargetIDs = pattern->getMirrorTargetIDs(); // NULL if not raid10

   size_t numTargetWorksHint = (maxUsedNodeIndex < 0) ? 0 : (maxUsedNodeIndex+1);
   size_t numTargetWorks = FHGFS_MIN(numTargetWorksHint, targetIDs->size() );
   size_t numMirrorWorks = mirrorTargetIDs ? numTargetWorks : 0;
   size_t numWorksTotal = numTargetWorks + numMirrorWorks;

   DynamicFileAttribsVec dynAttribsVec(targetIDs->size() ); // (only targets, not mirrors)

   FhgfsOpsErrVec nodeResults(numWorksTotal); // 1st half for targets, 2nd half for mirrors
   SynchronizedCounter counter;

   // generate work for storage targets...

   for(size_t i=0; i < numTargetWorks; i++)
   {
      CloseChunkFileWork* work = new CloseChunkFileWork(sessionID, fileHandleID,
         (*targetIDs)[i], &(dynAttribsVec[i]), &(nodeResults[i]), &counter);

      slaveQ->addDirectWork(work);
   }

   // generate work for mirror targets...

   for(size_t i=0; i < numMirrorWorks; i++)
   {
      CloseChunkFileWork* work = new CloseChunkFileWork(sessionID, fileHandleID,
         (*mirrorTargetIDs)[i], NULL, &(nodeResults[numTargetWorks + i] ), &counter);

      work->setIsMirror(true);

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
         if(nodeResults[i] == FhgfsOpsErr_INUSE)
            continue; // don't escalate this error to client (happens on ctrl+c)

         LogContext(logContext).log(Log_WARNING,
            "Problems occurred during release of storage server file handles. "
            "FileHandle: " + std::string(fileHandleID) );

         totalSuccess = false;
         goto apply_dyn_attribs;
      }
   }

   // check mirror results...

   for(size_t i=0; i < numMirrorWorks; i++)
   {
      if(unlikely(nodeResults[numTargetWorks + i] != FhgfsOpsErr_SUCCESS) )
      {
         if(nodeResults[numTargetWorks + i] == FhgfsOpsErr_INUSE)
            continue; // don't escalate this error to client (happens on ctrl+c)

         LogContext(logContext).log(Log_WARNING,
            "Problems occurred during release of storage server (mirror) file handles. "
            "FileHandle: " + std::string(fileHandleID) );

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
 * Unlink file in META_DISPOSALDIR_ID_STR/
 */
FhgfsOpsErr MsgHelperClose::unlinkDisposableFile(std::string fileID)
{
   // Note: This attempt to unlink directly is inefficient if the file is marked as disposable
   //    and is still busy (but we assume that this rarely happens)

   FhgfsOpsErr disposeRes = MsgHelperUnlink::unlinkFile(META_DISPOSALDIR_ID_STR, fileID);
   if(disposeRes == FhgfsOpsErr_PATHNOTEXISTS)
      return FhgfsOpsErr_SUCCESS; // file not marked for disposal => not an error

   return disposeRes;
}
