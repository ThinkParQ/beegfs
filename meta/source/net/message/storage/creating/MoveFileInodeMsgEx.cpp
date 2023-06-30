#include <common/net/message/storage/creating/MoveFileInodeRespMsg.h>
#include "MoveFileInodeMsgEx.h"

//HashDirLock MoveFileInodeMsgEx::lock(EntryLockStore& store)
std::tuple<FileIDLock, ParentNameLock, FileIDLock> MoveFileInodeMsgEx::lock(EntryLockStore& store)
{
   EntryInfo* fileInfo = getFromFileEntryInfo();

   FileIDLock dirLock(&store, fileInfo->getParentEntryID(), true);
   ParentNameLock dentryLock(&store, fileInfo->getParentEntryID(), fileInfo->getFileName());
   FileIDLock inodeLock(&store, fileInfo->getEntryID(), true);

   return std::make_tuple(
         std::move(dirLock),
         std::move(dentryLock),
         std::move(inodeLock));
}

bool MoveFileInodeMsgEx::processIncoming(ResponseContext& ctx)
{
   rctx = &ctx;
   return BaseType::processIncoming(ctx);
}

std::unique_ptr<MirroredMessageResponseState> MoveFileInodeMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();

   EntryInfo* fromFileInfo = getFromFileEntryInfo();
   DirInode* parentDir = app->getMetaStore()->referenceDir(
            fromFileInfo->getParentEntryID(), fromFileInfo->getIsBuddyMirrored(), true);

   if (getMode() == FileInodeMode::MODE_INVALID)
   {
      // invalid operation requested
      metaStore->releaseDir(parentDir->getID());
      return boost::make_unique<ResponseState>(FhgfsOpsErr_INTERNAL);
   }

   FhgfsOpsErr retVal = metaStore->verifyAndMoveFileInode(*parentDir, fromFileInfo, getMode());

   metaStore->releaseDir(parentDir->getID());
   return boost::make_unique<ResponseState>(retVal);
}

void MoveFileInodeMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_MoveFileInodeResp);
}
