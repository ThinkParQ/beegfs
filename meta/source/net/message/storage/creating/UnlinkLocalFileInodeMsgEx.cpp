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
   // Create a copy of the passed-in entryInfo and use it!
   // Reason:
   // Some fields of EntryInfo, such as parentEntryID and isInlined, can be modified
   // by MetaStore::unlinkInodeLater() during unlink processing. Using a copy ensures
   // that the original (and unmodified) entryInfo is forwarded to the secondary buddy
   // to avoid different states on meta-buddies.
   EntryInfo entryInfo;
   entryInfo.set(getDelEntryInfo());

   std::unique_ptr<FileInode> unlinkedInode;
   FhgfsOpsErr unlinkInodeRes = MsgHelperUnlink::unlinkFileInode(&entryInfo, &unlinkedInode);

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
