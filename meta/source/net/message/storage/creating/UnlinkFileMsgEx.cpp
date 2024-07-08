#include <common/components/streamlistenerv2/IncomingPreprocessedMsgWork.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/storage/creating/UnlinkFileRespMsg.h>
#include <common/net/message/storage/creating/UnlinkFileMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileRespMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileInodeMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileInodeRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <components/FileEventLogger.h>
#include <net/msghelpers/MsgHelperUnlink.h>
#include <program/Program.h>
#include "UnlinkFileMsgEx.h"

std::tuple<HashDirLock, FileIDLock, ParentNameLock, FileIDLock> UnlinkFileMsgEx::lock(EntryLockStore& store)
{
   HashDirLock hashLock;
   FileIDLock inodeLock;

   // we also have to lock the inode attached to the dentry - if we delete the inode, we must
   // exclude concurrent actions on the same inode. if we cannot look up a file inode for the
   // dentry, nothing bad happens.
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   auto dir = metaStore->referenceDir(getParentInfo()->getEntryID(),
                  getParentInfo()->getIsBuddyMirrored(), false);

   DirEntry dentry(getDelFileName());
   bool dentryExists = dir->getFileDentry(getDelFileName(), dentry);

   if (dentryExists)
   {
      dentry.getEntryInfo(getParentInfo()->getEntryID(), 0, &fileInfo);

      // lock hash dir where we are going to remove (or update) file inode
      // need to take it only if it is a non-inlined inode and resynch is running
      if (resyncJob && resyncJob->isRunning() && !dentry.getIsInodeInlined())
         hashLock = {&store, MetaStorageTk::getMetaInodeHash(dentry.getID())};
   }

   FileIDLock dirLock(&store, getParentInfo()->getEntryID(), true);
   ParentNameLock dentryLock(&store, getParentInfo()->getEntryID(), getDelFileName());

   if (dentryExists)
      inodeLock = {&store, dentry.getID(), true};

   metaStore->releaseDir(dir->getID());
   return std::make_tuple(std::move(hashLock), std::move(dirLock), std::move(dentryLock), std::move(inodeLock));
}

bool UnlinkFileMsgEx::processIncoming(ResponseContext& ctx)
{
#ifdef BEEGFS_DEBUG
   const char* logContext = "UnlinkFileMsg incoming";

   const std::string& removeName = getDelFileName();
   EntryInfo* parentInfo = getParentInfo();
   LOG_DEBUG(logContext, Log_DEBUG, "ParentID: " + parentInfo->getEntryID() + "; "
      "deleteName: " + removeName);
#endif // BEEGFS_DEBUG

   // update operation counters (here on top because we have an early sock release in this msg)
   updateNodeOp(ctx, MetaOpCounter_UNLINK);

   return BaseType::processIncoming(ctx);
}

std::unique_ptr<MirroredMessageResponseState> UnlinkFileMsgEx::executeLocally(
   ResponseContext& ctx, bool isSecondary)
{
   const char* logContext = "Unlink File Msg";
   App* app = Program::getApp();
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;
   MetaStore* metaStore = app->getMetaStore();
   EntryInfo delFileInfo;

   // reference parent dir
   DirInode* dir = metaStore->referenceDir(getParentInfo()->getEntryID(),
      getParentInfo()->getIsBuddyMirrored(), true);

   if (!dir)
      return boost::make_unique<ResponseState>(FhgfsOpsErr_PATHNOTEXISTS);

   DirEntry dentryToRemove(getDelFileName());
   if (!dir->getFileDentry(getDelFileName(), dentryToRemove))
   {
      metaStore->releaseDir(dir->getID());
      return boost::make_unique<ResponseState>(FhgfsOpsErr_PATHNOTEXISTS);
   }

   // On meta-mirrored setup, ensure the dentry we just loaded i.e. 'dentryToRemove' has the same entryID as 'fileInfo'.
   // This check is crucial to detect a very rare race condition where dentry gets unlinked
   // because the parent directory was not locked during UnlinkFileMsgEx::lock(),
   // and then reappears because the same file is being created again. If the entryIDs
   // don't match, it means the dentry has changed, and the exclusive lock taken on the
   // entryID before won't be effective in preventing future races of ongoing unlink
   // operation with other filesystem operations (e.g. close()) which might happen on this file.
   // Therefore, we release the directory from the meta store and return an error saying path
   // not exists.
   if (isMirrored() && (dentryToRemove.getEntryID() != this->fileInfo.getEntryID()))
   {
      metaStore->releaseDir(dir->getID());
      return boost::make_unique<ResponseState>(FhgfsOpsErr_PATHNOTEXISTS);
   }

   // get entryInfo
   dentryToRemove.getEntryInfo(getParentInfo()->getEntryID(), 0, &delFileInfo);

   // release dir
   metaStore->releaseDir(dir->getID());

   // check whether local node/group owns file's inode (dentry's owner may/maynot be same)
   NumNodeID ownerNodeID = delFileInfo.getOwnerNodeID();
   if ( (!isMirrored() && ownerNodeID == app->getLocalNode().getNumID()) ||
      (isMirrored() &&
         ownerNodeID.val() == app->getMetaBuddyGroupMapper()->getLocalGroupID()) )
   {
      // file's inode is present on local meta node/buddy-group
      if (isSecondary)
         return executeSecondary(ctx);
      else
         return executePrimary(ctx);
   }
   else
   {
      // file's inode is on some different node/buddy-group

      // reference parent dir again
      DirInode* parentDir = metaStore->referenceDir(getParentInfo()->getEntryID(),
         getParentInfo()->getIsBuddyMirrored(), true);
      if (!parentDir)
         return boost::make_unique<ResponseState>(FhgfsOpsErr_PATHNOTEXISTS);

      // dirEntry already loaded before - now remove it
      retVal = parentDir->unlinkDirEntry(getDelFileName(), &dentryToRemove,
                  DirEntry_UNLINK_FILENAME);

      // release dir
      metaStore->releaseDir(parentDir->getID());

      if (retVal != FhgfsOpsErr_SUCCESS)
         return boost::make_unique<ResponseState>(retVal);

      if (!isSecondary)
      {
         // now remove inode from remote meta node
         UnlinkLocalFileInodeMsg unlinkInodeMsg(&delFileInfo);

         RequestResponseArgs rrArgs(NULL, &unlinkInodeMsg, NETMSGTYPE_UnlinkLocalFileInodeResp);
         RequestResponseNode rrNode(ownerNodeID, app->getMetaNodes());
         rrNode.setTargetStates(app->getMetaStateStore());

         if (fileInfo.getIsBuddyMirrored())
         {
            rrNode.setMirrorInfo(app->getMetaBuddyGroupMapper(), false);
         }

         do
         {
            FhgfsOpsErr resp = MessagingTk::requestResponseNode(&rrNode, &rrArgs);

            if (unlikely(resp != FhgfsOpsErr_SUCCESS))
            {
               LogContext(logContext).log(Log_WARNING,
                  "Communication with metadata server failed. "
                  "nodeID: " + ownerNodeID.str() + "; " +
                  "entryID: " + fileInfo.getEntryID().c_str());
               break;
            }

            // response received
            const auto unlinkFileInodeRespMsg = (UnlinkLocalFileInodeRespMsg*) rrArgs.outRespMsg.get();
            FhgfsOpsErr res = unlinkFileInodeRespMsg->getResult();
            if (res != FhgfsOpsErr_SUCCESS)
            {
               // error: either inode file doesn't exists or some other error happened
               LogContext(logContext).log(Log_WARNING, "unlink file inode failed! "
                  "nodeID: " + ownerNodeID.str() + "; " +
                  "entryID: " + fileInfo.getEntryID().c_str());
               break;
            }

            // since dentry has been removed successfully so all good for user - no need
            // to return an error if unlinking remote inode fails due to some reasons.
            // we still need to remove dentry from secondary buddy so should not overwrite
            // local dentry removal success with some remote error
            retVal = FhgfsOpsErr_SUCCESS;
         } while (false);
      }

      return boost::make_unique<ResponseState>(retVal);
   }

   // added to avoid compiler warnings
   return boost::make_unique<ResponseState>(FhgfsOpsErr_SUCCESS);
}

std::unique_ptr<UnlinkFileMsgEx::ResponseState> UnlinkFileMsgEx::executePrimary(
   ResponseContext& ctx)
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();

   // we do not need to load the directory here - if the dentry does not exist, it will not be
   // modified. if the dentry does exist, it will be loaded.
   DirInode* dir = app->getMetaStore()->referenceDir(getParentInfo()->getEntryID(),
      getParentInfo()->getIsBuddyMirrored(), false);
   if (!dir)
      return boost::make_unique<ResponseState>(FhgfsOpsErr_PATHNOTEXISTS);

   const bool fileEventLoggingEnabled = getFileEvent() && app->getFileEventLogger();

   // the entryID lookup here is done again in unlinkMetaFile() below, but not returend.
   const auto getEntryId = [=] () {
      EntryInfo entryInfo;
      dir->getFileEntryInfo(getDelFileName(), entryInfo);
      return entryInfo.getEntryID();
    };
   const auto entryIdBeforeUnlink = fileEventLoggingEnabled ? getEntryId() : "";

   /* two alternatives:
          1) early response before chunk files unlink.
          2) normal response after chunk files unlink (incl. chunk files error). */

   if(cfg->getTuneEarlyUnlinkResponse() && !isMirrored())
   {
      // alternative 1: response before chunk files unlink
      std::unique_ptr<FileInode> unlinkedInode;

      FhgfsOpsErr unlinkMetaRes = MsgHelperUnlink::unlinkMetaFile(*dir,
         getDelFileName(), &unlinkedInode);

      app->getMetaStore()->releaseDir(dir->getID());
      earlyComplete(ctx, ResponseState(unlinkMetaRes));

      /* note: if the file is still opened or if there were hardlinks then unlinkedInode will be
         NULL even on FhgfsOpsErr_SUCCESS */
      if( (unlinkMetaRes == FhgfsOpsErr_SUCCESS) && unlinkedInode)
         MsgHelperUnlink::unlinkChunkFiles(unlinkedInode.release(), getMsgHeaderUserID() );

      if (unlinkMetaRes == FhgfsOpsErr_SUCCESS && app->getFileEventLogger())
      {
         if (getFileEvent())
         {
            auto& ev = *getFileEvent();
            app->getFileEventLogger()->log(ev,
                                        entryIdBeforeUnlink,
                                        getParentInfo()->getEntryID());
         }

      }

      return {};
   }

   // alternative 2: response after chunk files unlink
   std::unique_ptr<FileInode> unlinkedInode;
   FhgfsOpsErr unlinkRes = MsgHelperUnlink::unlinkMetaFile(*dir,
      getDelFileName(), &unlinkedInode);

   if (unlinkRes == FhgfsOpsErr_SUCCESS && shouldFixTimestamps())
   {
      fixInodeTimestamp(*dir, dirTimestamps);

      if (!unlinkedInode)
      {
         auto file = app->getMetaStore()->referenceFile(&fileInfo);
         if (file)
         {
            fixInodeTimestamp(*file, fileTimestamps, &fileInfo);
            app->getMetaStore()->releaseFile(dir->getID(), file);
         }
      }
   }

   /* note: if the file is still opened or if there are/were hardlinks then unlinkedInode will be
      NULL even on FhgfsOpsErr_SUCCESS */
   if( (unlinkRes == FhgfsOpsErr_SUCCESS) && unlinkedInode)
      MsgHelperUnlink::unlinkChunkFiles(unlinkedInode.release(), getMsgHeaderUserID());

   app->getMetaStore()->releaseDir(dir->getID());

   if (unlinkRes == FhgfsOpsErr_SUCCESS && app->getFileEventLogger() && getFileEvent())
   {
         app->getFileEventLogger()->log(
                  *getFileEvent(),
                  entryIdBeforeUnlink,
                  getParentInfo()->getEntryID());
   }

   return boost::make_unique<ResponseState>(unlinkRes);
}

std::unique_ptr<UnlinkFileMsgEx::ResponseState> UnlinkFileMsgEx::executeSecondary(
   ResponseContext& ctx)
{
   MetaStore* const metaStore = Program::getApp()->getMetaStore();
   std::unique_ptr<FileInode> unlinkedInode;

   // we do not need to load the directory here - if the dentry does not exist, it will not be
   // modified. if the dentry does exist, it will be loaded.
    DirInode* dir = metaStore->referenceDir(getParentInfo()->getEntryID(),
          getParentInfo()->getIsBuddyMirrored(), false);
    if (!dir)
       return boost::make_unique<ResponseState>(FhgfsOpsErr_PATHNOTEXISTS);

   FhgfsOpsErr unlinkMetaRes = MsgHelperUnlink::unlinkMetaFile(*dir,
      getDelFileName(), &unlinkedInode);

   if (unlinkMetaRes == FhgfsOpsErr_SUCCESS && shouldFixTimestamps())
   {
      fixInodeTimestamp(*dir, dirTimestamps);

      if (!unlinkedInode)
      {
         auto file = metaStore->referenceFile(&fileInfo);
         if (file)
         {
            fixInodeTimestamp(*file, fileTimestamps, &fileInfo);
            metaStore->releaseFile(dir->getID(), file);
         }
      }
   }

   metaStore->releaseDir(dir->getID());

   return boost::make_unique<ResponseState>(unlinkMetaRes);
}

void UnlinkFileMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_UnlinkFileResp);
}
