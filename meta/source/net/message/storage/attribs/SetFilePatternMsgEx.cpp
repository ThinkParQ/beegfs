#include <session/EntryLock.h>
#include "SetFilePatternMsgEx.h"

bool SetFilePatternMsgEx::processIncoming(ResponseContext& ctx)
{
   #ifdef BEEGFS_DEBUG
      const char* logContext = "SetFilePatternMsgEx incoming";

      EntryInfo* entryInfo = this->getEntryInfo();
      LOG_DEBUG(logContext, 5, "EntryID: " + entryInfo->getEntryID() +
         "; FileName: " + entryInfo->getFileName() +
         "; EntryType: " + StringTk::intToStr(entryInfo->getEntryType()) +
         "; isBuddyMirrored: " + StringTk::intToStr(entryInfo->getIsBuddyMirrored()) +
         "; remoteStorageTarget (to be set) info: " + getRemoteStorageTarget()->toStr());
   #endif

   BaseType::processIncoming(ctx);
   return true;
}

FileIDLock SetFilePatternMsgEx::lock(EntryLockStore& store)
{
   return {&store, getEntryInfo()->getEntryID(), true};
}

std::unique_ptr<MirroredMessageResponseState> SetFilePatternMsgEx::executeLocally(
   ResponseContext& ctx, bool isSecondary)
{
   const char* logContext = "Set File Pattern";
   FhgfsOpsErr res = FhgfsOpsErr_SUCCESS;

   MetaStore* metaStore = Program::getApp()->getMetaStore();

   EntryInfo* entryInfo = this->getEntryInfo();
   RemoteStorageTarget* rst = this->getRemoteStorageTarget();

   auto [inode, referenceRes] = metaStore->referenceFile(entryInfo);
   if (unlikely(!inode))
      return boost::make_unique<ResponseState>(referenceRes);

   // Ignore if the request did not contain RST configuration:
   if (!rst->hasInvalidVersion())
   {
      auto const& rstIDs = rst->getRstIdVector();
      if (rstIDs.empty())
      {
         // Empty RST ID list indicates a request to clear/unset RSTs for this file.
         res = inode->clearRemoteStorageTarget(entryInfo);
         if (res != FhgfsOpsErr_SUCCESS)
         {
            LogContext(logContext).logErr("Failed to clear RST info; entryID: " +
               entryInfo->getEntryID());
         }
      }
      else
      {
         res = inode->setRemoteStorageTarget(entryInfo, *rst);
         if (res != FhgfsOpsErr_SUCCESS)
         {
             LogContext(logContext).logErr("Setting RST info failed; entryID: " +
               entryInfo->getEntryID());
         }
      }
   }

   metaStore->releaseFile(entryInfo->getParentEntryID(), inode);
   return boost::make_unique<ResponseState>(res);
}

void SetFilePatternMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_SetFilePatternResp);
}
