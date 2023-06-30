#include <common/toolkit/MessagingTk.h>
#include <common/net/message/storage/creating/UnlinkLocalFileMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileRespMsg.h>
#include <components/ModificationEventFlusher.h>
#include <components/worker/UnlinkChunkFileWork.h>
#include <net/msghelpers/MsgHelperMkFile.h>
#include <program/Program.h>
#include "MsgHelperUnlink.h"


/**
 * Wrapper for unlinkMetaFile() and unlinkChunkFiles().
 *
 * @param msgUserID only used in msg header info.
 */
FhgfsOpsErr MsgHelperUnlink::unlinkFile(DirInode& parentDir, const std::string& removeName,
   unsigned msgUserID)
{
   std::unique_ptr<FileInode> unlinkedInode;

   FhgfsOpsErr unlinkMetaRes = unlinkMetaFile(parentDir, removeName, &unlinkedInode);

   /* note: if the file is still opened or if there are/were hardlinks then unlinkedInode will be
      NULL even on FhgfsOpsErr_SUCCESS */
   if (unlinkMetaRes == FhgfsOpsErr_SUCCESS && unlinkedInode)
      unlinkMetaRes = unlinkChunkFiles(unlinkedInode.release(), msgUserID);

   return unlinkMetaRes;
}

/**
 * Unlink file in metadata store.
 *
 * @return if this returns success and outUnlinkedFile is set, then the caller also needs to unlink
 * the chunk files via unlinkChunkFiles().
 */
FhgfsOpsErr MsgHelperUnlink::unlinkMetaFile(DirInode& parentDir,
   const std::string& removeName, std::unique_ptr<FileInode>* outUnlinkedFile)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   ModificationEventFlusher* modEventFlusher = Program::getApp()->getModificationEventFlusher();
   bool modEventLoggingEnabled = modEventFlusher->isLoggingEnabled();

   EntryInfo entryInfo;

   FhgfsOpsErr unlinkMetaRes = metaStore->unlinkFile(parentDir, removeName,
      &entryInfo, outUnlinkedFile);

   if (modEventLoggingEnabled)
   {
      std::string entryID = entryInfo.getEntryID();
      modEventFlusher->add(ModificationEvent_FILEREMOVED, entryID);
   }

   return unlinkMetaRes;
}

/**
 * Decrement hardlink count and
 * Unlink file's inode if hardlink count reaches zero
 *
 * @return Success if hardlink count decrement is successful
 * and if it became zero then fileinode removal is also sucessful (outUnlinkInode will be
 * set in this case which will be later used to remove chunk files). If file is in use and
 * its last entry getting removed then inode will be linked with disposal directory for later
 * removal
 *
 */
FhgfsOpsErr MsgHelperUnlink::unlinkFileInode(EntryInfo* delFileInfo,
               std::unique_ptr<FileInode>* outUnlinkedFile)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   FhgfsOpsErr unlinkRes = metaStore->unlinkFileInode(delFileInfo, outUnlinkedFile);
   return unlinkRes;
}

/**
 * Unlink (storage) chunk files.
 *
 * Note: If chunk files unlink fails, this method will create a disposal entry.
 *
 * @param unlinkedInode will be deleted inside this method or owned by another object, so caller
 * may no longer access it after calling this.
 * @param msgUserID only used in msg header info.
 */
FhgfsOpsErr MsgHelperUnlink::unlinkChunkFiles(FileInode* unlinkedInode, unsigned msgUserID)
{
   const char* logContext = "Delete chunk files";
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   FhgfsOpsErr retVal;

   retVal = unlinkChunkFilesInternal(*unlinkedInode, msgUserID);
   if(retVal != FhgfsOpsErr_SUCCESS)
   {  /* Failed to unlink storage chunk files => add file to the disposable store to try
       * again later. */

      LogContext(logContext).logErr("Failed to delete all chunk files of ID: " +
         unlinkedInode->getEntryID() + ". Added disposal entry.");

      retVal = metaStore->insertDisposableFile(unlinkedInode); // destructs unlinkedInode
   }
   else
   { // success (local files unlinked)
      delete(unlinkedInode);
   }

   return retVal;
}

/**
 * Wrapper to decide parallel or sequential chunks unlink.
 *
 * @param msgUserID only used in msg header info.
 */
FhgfsOpsErr MsgHelperUnlink::unlinkChunkFilesInternal(FileInode& file, unsigned msgUserID)
{
   StripePattern* pattern = file.getStripePattern();

   if( (pattern->getStripeTargetIDs()->size() > 1) ||
       (pattern->getPatternType() == StripePatternType_BuddyMirror) )
      return unlinkChunkFileParallel(file, msgUserID);
   else
      return unlinkChunkFileSequential(file, msgUserID);
}

/**
 * Note: This method does not work for mirrored files; use unlinkChunkFileParallel() for those.
 *
 * @param msgUserID only used in msg header info.
 */
FhgfsOpsErr MsgHelperUnlink::unlinkChunkFileSequential(FileInode& inode, unsigned msgUserID)
{
   std::string logContext("Unlink Helper (unlink chunk file S [" + inode.getEntryID() + "])");

   StripePattern* pattern = inode.getStripePattern();
   const UInt16Vector* targetIDs = pattern->getStripeTargetIDs();
   TargetMapper* targetMapper = Program::getApp()->getTargetMapper();
   TargetStateStore* targetStates = Program::getApp()->getTargetStateStore();
   NodeStore* nodes = Program::getApp()->getStorageNodes();

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;
   std::string fileID(inode.getEntryID());

   PathInfo pathInfo;
   inode.getPathInfo(&pathInfo);

   // send request to each node and receive the response message
   for(UInt16VectorConstIter iter = targetIDs->begin();
      iter != targetIDs->end();
      iter++)
   {
      uint16_t targetID = *iter;

      UnlinkLocalFileMsg unlinkMsg(fileID, targetID, &pathInfo);

      unlinkMsg.setMsgHeaderUserID(msgUserID);

      RequestResponseArgs rrArgs(NULL, &unlinkMsg, NETMSGTYPE_UnlinkLocalFileResp);
      RequestResponseTarget rrTarget(targetID, targetMapper, nodes);

      rrTarget.setTargetStates(targetStates);

      // send request to node and receive response
      FhgfsOpsErr requestRes = MessagingTk::requestResponseTarget(&rrTarget, &rrArgs);

      if(requestRes != FhgfsOpsErr_SUCCESS)
      { // communication failed

         if( (requestRes == FhgfsOpsErr_UNKNOWNNODE) ||
             (requestRes == FhgfsOpsErr_UNKNOWNTARGET) )
         { /* special case: for unlink, we don't treat this as error to allow easy deletion of
              files after intentional target removal. */
            LogContext(logContext).log(Log_WARNING,
               "Unable to resolve storage node targetID: " + StringTk::uintToStr(targetID) );
            continue;
         }

         LogContext(logContext).log(Log_WARNING,
            "Communication with storage target failed: " + StringTk::uintToStr(targetID) + "; "
            "fileID: " + inode.getEntryID());

         if(retVal == FhgfsOpsErr_SUCCESS)
            retVal = requestRes;

         continue;
      }

      // correct response type received
      UnlinkLocalFileRespMsg* unlinkRespMsg = (UnlinkLocalFileRespMsg*)rrArgs.outRespMsg.get();

      FhgfsOpsErr unlinkResult = unlinkRespMsg->getResult();
      if(unlinkResult != FhgfsOpsErr_SUCCESS)
      { // error: local inode not unlinked
         LogContext(logContext).log(Log_WARNING,
            "Storage target failed to unlink chunk file: " + StringTk::uintToStr(targetID) + "; "
            "fileID: " + inode.getEntryID());

         if(retVal == FhgfsOpsErr_SUCCESS)
            retVal = unlinkResult;

         continue;
      }

      // success: local inode unlinked
      LOG_DEBUG(logContext, Log_DEBUG,
         "Storage targed unlinked chunk file: " + StringTk::uintToStr(targetID) + "; "
         "fileID: " + inode.getEntryID());
   }

   if(unlikely(retVal != FhgfsOpsErr_SUCCESS) )
      LogContext(logContext).log(Log_WARNING,
         "Problems occurred during unlinking of the chunk files. "
         "fileID: " + inode.getEntryID());


   return retVal;
}

/**
 * @param msgUserID only used in msg header info.
 */
FhgfsOpsErr MsgHelperUnlink::unlinkChunkFileParallel(FileInode& inode, unsigned msgUserID)
{
   std::string logContext("Unlink Helper (unlink chunk file [" + inode.getEntryID() + "])");

   App* app = Program::getApp();
   MultiWorkQueue* slaveQ = app->getCommSlaveQueue();
   StripePattern* pattern = inode.getStripePattern();
   const UInt16Vector* targetIDs = pattern->getStripeTargetIDs();

   size_t numTargetWorks = targetIDs->size();

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;
   FhgfsOpsErrVec nodeResults(numTargetWorks);
   SynchronizedCounter counter;

   PathInfo pathInfo;
   inode.getPathInfo(&pathInfo);

   // generate work for storage targets...

   for(size_t i=0; i < numTargetWorks; i++)
   {
      UnlinkChunkFileWork* work = new UnlinkChunkFileWork(inode.getEntryID(), pattern,
         (*targetIDs)[i], &pathInfo, &(nodeResults[i]), &counter);

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
         if( (nodeResults[i] == FhgfsOpsErr_UNKNOWNNODE) ||
             (nodeResults[i] == FhgfsOpsErr_UNKNOWNTARGET) )
         { /* we don't return this as an error to the user, because the node/target was probably
              removed intentionally (and either way the rest of this inode is lost now) */
            continue;
         }

         LogContext(logContext).log(Log_WARNING,
            "Problems occurred during unlinking of chunk files.");

         retVal = nodeResults[i];
         goto error_exit;
      }
   }


error_exit:
   return retVal;
}
