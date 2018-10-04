#include <common/net/message/control/GenericResponseMsg.h>
#include <program/Program.h>
#include <common/net/message/storage/creating/MkLocalDirRespMsg.h>

#include "MkLocalDirMsgEx.h"

HashDirLock MkLocalDirMsgEx::lock(EntryLockStore& store)
{
   // we usually need not lock anything here, because the inode ID will be completely unknown to
   // anyone until we finish processing here *and* on the metadata server that sent this message.
   // during resync though we need to lock the hash dir to avoid interefence between bulk resync and
   // mod resync.

   // do not lock the hash dir if we are creating the inode on the same meta node as the dentry,
   // MkDir will have already locked the hash dir.
   if (!rctx->isLocallyGenerated() && resyncJob && resyncJob->isRunning())
      return {&store, MetaStorageTk::getMetaInodeHash(getEntryInfo()->getEntryID())};

   return {};
}

bool MkLocalDirMsgEx::processIncoming(ResponseContext& ctx)
{
   EntryInfo* entryInfo = getEntryInfo();

   LOG_DBG(GENERAL, DEBUG, "", entryInfo->getEntryID(), entryInfo->getFileName());
   (void) entryInfo;

   rctx = &ctx;

   return BaseType::processIncoming(ctx);
}

std::unique_ptr<MirroredMessageResponseState> MkLocalDirMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();
   StripePattern& pattern = getPattern();

   EntryInfo *entryInfo = getEntryInfo();
   NumNodeID parentNodeID = getParentNodeID();

   NumNodeID ownerNodeID = entryInfo->getIsBuddyMirrored()
      ? NumNodeID(app->getMetaBuddyGroupMapper()->getLocalGroupID() )
      : app->getLocalNode().getNumID();

   DirInode newDir(entryInfo->getEntryID(), getMode(), getUserID(),
      getGroupID(), ownerNodeID, pattern, entryInfo->getIsBuddyMirrored());

   newDir.setParentInfoInitial(entryInfo->getParentEntryID(), parentNodeID);

   FhgfsOpsErr mkRes = metaStore->makeDirInode(newDir, getDefaultACLXAttr(), getAccessACLXAttr() );

   if (mkRes == FhgfsOpsErr_SUCCESS && shouldFixTimestamps())
      fixInodeTimestamp(newDir, dirTimestamps);

   return boost::make_unique<ResponseState>(mkRes);
}

void MkLocalDirMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_MkLocalDirResp);
}
