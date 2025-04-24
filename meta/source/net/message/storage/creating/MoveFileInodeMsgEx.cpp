#include <common/net/message/storage/creating/MoveFileInodeRespMsg.h>
#include "MoveFileInodeMsgEx.h"

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
   MoveFileInodeMsgResponseState resp;

   if (getMode() == FileInodeMode::MODE_INVALID)
   {
      // invalid operation requested
      return boost::make_unique<ResponseState>(std::move(resp));
   }

   EntryInfo* fromFileInfo = getFromFileEntryInfo();
   FhgfsOpsErr retVal = FhgfsOpsErr_INTERNAL;
   unsigned linkCount = 0;

   if (getCreateHardlink())
   {
      std::tie(retVal, linkCount) = metaStore->makeNewHardlink(fromFileInfo);
   }
   else
   {
      DirInode* parentDir = metaStore->referenceDir(
         fromFileInfo->getParentEntryID(), fromFileInfo->getIsBuddyMirrored(), true);

      if (likely(parentDir))
      {
         retVal = metaStore->verifyAndMoveFileInode(*parentDir, fromFileInfo, getMode());
         metaStore->releaseDir(parentDir->getID());
      }
      else
         retVal = FhgfsOpsErr_PATHNOTEXISTS;
   }

   resp.setResult(retVal);
   resp.setHardlinkCount(linkCount);
   return boost::make_unique<ResponseState>(std::move(resp));
}

void MoveFileInodeMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_MoveFileInodeResp);
}
