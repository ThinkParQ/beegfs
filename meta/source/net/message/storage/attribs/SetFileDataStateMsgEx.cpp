#include <session/EntryLock.h>
#include "SetFileDataStateMsgEx.h"

bool SetFileDataStateMsgEx::processIncoming(ResponseContext& ctx)
{
   #ifdef BEEGFS_DEBUG
      const char* logContext = "SetFileDataStateMsgEx incoming";

      EntryInfo* entryInfo = this->getEntryInfo();
      LOG_DEBUG(logContext, 5, "EntryID: " + entryInfo->getEntryID() +
         "; FileName: " + entryInfo->getFileName() +
         "; EntryType: " + StringTk::intToStr(entryInfo->getEntryType()) +
         "; isBuddyMirrored: " + StringTk::intToStr(entryInfo->getIsBuddyMirrored()) +
         "; file data state (to be set): " + StringTk::intToStr(this->getFileDataState()));
   #endif

   BaseType::processIncoming(ctx);
   return true;
}

FileIDLock SetFileDataStateMsgEx::lock(EntryLockStore& store)
{
   return {&store, getEntryInfo()->getEntryID(), true};
}

std::unique_ptr<MirroredMessageResponseState> SetFileDataStateMsgEx::executeLocally(
   ResponseContext& ctx, bool isSecondary)
{
   const char* logContext = "Set File Data State";
   FhgfsOpsErr res = FhgfsOpsErr_INTERNAL;

   MetaStore* metaStore = Program::getApp()->getMetaStore();
   EntryInfo* entryInfo = this->getEntryInfo();

   auto [inode, referenceRes] = metaStore->referenceFile(entryInfo);
   if (unlikely(!inode))
      return boost::make_unique<ResponseState>(referenceRes);

   inode->setFileDataState(this->getFileDataState());
   if (inode->updateInodeOnDisk(entryInfo))
      res = FhgfsOpsErr_SUCCESS;

   if (res != FhgfsOpsErr_SUCCESS)
   {
      LogContext(logContext).logErr("Setting file data state failed; entryID: " + entryInfo->getEntryID());
   }

   metaStore->releaseFile(entryInfo->getParentEntryID(), inode);
   return boost::make_unique<ResponseState>(res);
}

void SetFileDataStateMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_SetFileDataStateResp);
}