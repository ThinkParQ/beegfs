#include <common/components/streamlistenerv2/IncomingPreprocessedMsgWork.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/storage/creating/UnlinkFileRespMsg.h>
#include <common/net/message/storage/creating/UnlinkFileMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <components/FileEventLogger.h>
#include <net/msghelpers/MsgHelperUnlink.h>
#include <program/Program.h>
#include "UnlinkFileMsgEx.h"

std::tuple<FileIDLock, ParentNameLock, FileIDLock> UnlinkFileMsgEx::lock(EntryLockStore& store)
{
   FileIDLock dirLock(&store, getParentInfo()->getEntryID(), true);
   ParentNameLock dentryLock(&store, getParentInfo()->getEntryID(), getDelFileName());
   // we also have to lock the inode attached to the dentry - if we delete the inode, we must
   // exclude concurrent actions on the same inode. if we cannot look up a file inode for the
   // dentry, nothing bad happens.
   FileIDLock inodeLock;

   MetaStore* metaStore = Program::getApp()->getMetaStore();

   auto dir = metaStore->referenceDir(getParentInfo()->getEntryID(),
               getParentInfo()->getIsBuddyMirrored(), false);

   DirEntry dentry(getDelFileName());
   if (dir->getFileDentry(getDelFileName(), dentry))
   {
      dentry.getEntryInfo(getParentInfo()->getEntryID(), 0, &fileInfo);
      inodeLock = {&store, dentry.getID(), true};
   }

   metaStore->releaseDir(dir->getID());

   return std::make_tuple(std::move(dirLock), std::move(dentryLock), std::move(inodeLock));
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
   if (isSecondary)
      return executeSecondary(ctx);
   else
      return executePrimary(ctx);
}

std::unique_ptr<UnlinkFileMsgEx::ResponseState> UnlinkFileMsgEx::executePrimary(
   ResponseContext& ctx)
{
   /*
    *  note: in case of mirroring we need to
    *  1. lock parentDir/name combination
    *  2. acquire a read lock on the parentDirID (to make sure it won't be deleted)
    *  3. read the ID and lock it
    *  4. unlink the file
    *  5. forward to secondary
    *  6. unlock all locks
    */
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
    { // alternative 1: response before chunk files unlink
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
