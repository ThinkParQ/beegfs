#include <program/Program.h>
#include <common/net/message/storage/creating/RmLocalDirRespMsg.h>
#include <common/toolkit/MetaStorageTk.h>

#include "RmLocalDirMsgEx.h"


std::tuple<HashDirLock, FileIDLock> RmLocalDirMsgEx::lock(EntryLockStore& store)
{
   HashDirLock hashLock;

   // if we are currently under modsync, we must lock the hash dir from which we are removing the
   // inode. otherwise bulk sync may interfere with mod sync and cause the resync to fail.
   // do not lock the hash dir if we are removing the inode from the same meta node as the dentry,
   // RmDir will have already locked the hash dir.
   if (!rctx->isLocallyGenerated() && resyncJob && resyncJob->isRunning())
      hashLock = {&store, MetaStorageTk::getMetaInodeHash(getDelEntryInfo()->getEntryID())};

   return std::make_tuple(
         std::move(hashLock),
         FileIDLock(&store, getDelEntryInfo()->getEntryID(), true));
}

bool RmLocalDirMsgEx::processIncoming(ResponseContext& ctx)
{
   rctx = &ctx;

   return BaseType::processIncoming(ctx);
}

std::unique_ptr<MirroredMessageResponseState> RmLocalDirMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   return rmDir();
}

std::unique_ptr<RmLocalDirMsgEx::ResponseState> RmLocalDirMsgEx::rmDir()
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   EntryInfo* delEntryInfo = this->getDelEntryInfo();

   LOG_DEBUG("RmLocalDirMsgEx (rmDir)", Log_DEBUG,
      "Removing local dir inode: " + delEntryInfo->getFileName() + "; isBuddyMirrored: " +
      StringTk::intToStr(delEntryInfo->getIsBuddyMirrored()) );

   FhgfsOpsErr res = metaStore->removeDirInode(delEntryInfo->getEntryID(),
         delEntryInfo->getIsBuddyMirrored());

   return boost::make_unique<ResponseState>(res);
}

void RmLocalDirMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_RmLocalDirResp);
}
