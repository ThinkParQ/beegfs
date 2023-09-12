#include <program/Program.h>
#include <common/toolkit/MetaStorageTk.h>
#include <net/msghelpers/MsgHelperUnlink.h>
#include "UnlinkLocalFileInodeMsgEx.h"


std::tuple<HashDirLock, FileIDLock> UnlinkLocalFileInodeMsgEx::lock(EntryLockStore& store)
{
   // we must not lock hash dir and inode if it is owned by current node. if it is a locally
   // generated message which is sent by unlinkmsg and renamev2msg sends then aforementioned
   // locks are already held by them
   if (rctx->isLocallyGenerated())
      return {};

   HashDirLock hashLock;

   if (resyncJob && resyncJob->isRunning())
      HashDirLock hashLock = {&store, MetaStorageTk::getMetaInodeHash(getDelEntryInfo()->getEntryID())};

   return std::make_tuple(
         std::move(hashLock),
         FileIDLock(&store, getDelEntryInfo()->getEntryID(), true));
}

bool UnlinkLocalFileInodeMsgEx::processIncoming(ResponseContext& ctx)
{
   rctx = &ctx;
   return BaseType::processIncoming(ctx);
}

std::unique_ptr<MirroredMessageResponseState> UnlinkLocalFileInodeMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();
   EntryInfo* delFileInfo = getDelEntryInfo();

   MetaFileHandle inode = metaStore->referenceFile(delFileInfo);

   if  (!inode)
      return boost::make_unique<ResponseState>(FhgfsOpsErr_PATHNOTEXISTS);

   metaStore->releaseFile(delFileInfo->getParentEntryID(), inode);

   std::unique_ptr<FileInode> unlinkedInode;
   FhgfsOpsErr unlinkInodeRes = MsgHelperUnlink::unlinkFileInode(delFileInfo, &unlinkedInode);

   if ((unlinkInodeRes == FhgfsOpsErr_SUCCESS) && unlinkedInode && !isSecondary)
   {
      MsgHelperUnlink::unlinkChunkFiles(unlinkedInode.release(), getMsgHeaderUserID());
   }

   return boost::make_unique<ResponseState>(unlinkInodeRes);
}

void UnlinkLocalFileInodeMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_UnlinkLocalFileInodeResp);
}
