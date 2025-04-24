#include "BumpFileVersionMsgEx.h"

#include <components/FileEventLogger.h>
#include <program/Program.h>

bool BumpFileVersionMsgEx::processIncoming(ResponseContext& ctx)
{
   LOG_DBG(SESSIONS, DEBUG, "", getEntryInfo().getEntryID(), getEntryInfo().getIsBuddyMirrored(),
         hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond),
         isMsgHeaderFeatureFlagSet(BUMPFILEVERSIONMSG_FLAG_PERSISTENT),
         isMsgHeaderFeatureFlagSet(BUMPFILEVERSIONMSG_FLAG_HASEVENT));
   if (isMsgHeaderFeatureFlagSet(BUMPFILEVERSIONMSG_FLAG_HASEVENT))
      LOG_DBG(SESSIONS, DEBUG, "", int(getFileEvent()->type), getFileEvent()->path);

   return BaseType::processIncoming(ctx);
}

FileIDLock BumpFileVersionMsgEx::lock(EntryLockStore& store)
{
   return {&store, getEntryInfo().getEntryID(), true};
}

std::unique_ptr<MirroredMessageResponseState> BumpFileVersionMsgEx::executeLocally(
   ResponseContext& ctx, bool isSecondary)
{
   auto* app = Program::getApp();
   auto* metaStore = app->getMetaStore();

   auto [inode, referenceRes] = metaStore->referenceFile(&getEntryInfo());
   if (!inode)
      return boost::make_unique<ResponseState>(FhgfsOpsErr_INTERNAL);

   if (isMsgHeaderFeatureFlagSet(BUMPFILEVERSIONMSG_FLAG_PERSISTENT) &&
         !inode->incrementFileVersion(&getEntryInfo()))
   {
      metaStore->releaseFile(getEntryInfo().getParentEntryID(), inode);
      return boost::make_unique<ResponseState>(FhgfsOpsErr_SAVEERROR);
   }

   if (!isSecondary && app->getFileEventLogger() && getFileEvent())
   {
      EventContext eventCtx = makeEventContext(
         &getEntryInfo(),
         getEntryInfo().getParentEntryID(),
         getMsgHeaderUserID(),
         "",
         inode->getNumHardlinks(),
         isSecondary
      );

      logEvent(app->getFileEventLogger(), *getFileEvent(), eventCtx);
   }

   metaStore->releaseFile(getEntryInfo().getParentEntryID(), inode);
   return boost::make_unique<ResponseState>(FhgfsOpsErr_SUCCESS);
}

void BumpFileVersionMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   unsetMsgHeaderFeatureFlag(BUMPFILEVERSIONMSG_FLAG_HASEVENT);
   sendToSecondary(ctx, *this, NETMSGTYPE_BumpFileVersionResp);
}
