#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/storage/TruncFileRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <components/FileEventLogger.h>
#include <net/msghelpers/MsgHelperTrunc.h>
#include "TruncFileMsgEx.h"
#include <program/Program.h>

FileIDLock TruncFileMsgEx::lock(EntryLockStore& store)
{
   return {&store, getEntryInfo()->getEntryID()};
}

bool TruncFileMsgEx::processIncoming(ResponseContext& ctx)
{
#ifdef BEEGFS_DEBUG
   const char* logContext = "TruncFileMsgEx incoming";

   LOG_DEBUG(logContext, Log_DEBUG, "Received a TruncFileMsg from: " + ctx.peerName() );
#endif // BEEGFS_DEBUG

   return BaseType::processIncoming(ctx);
}

std::unique_ptr<MirroredMessageResponseState> TruncFileMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   App* app = Program::getApp();
   // update operation counters
   Program::getApp()->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(),
      MetaOpCounter_TRUNCATE, getMsgHeaderUserID() );

   MetaStore* metaStore = Program::getApp()->getMetaStore();

   if (!isSecondary)
   {
      FhgfsOpsErr truncRes = MsgHelperTrunc::truncFile(getEntryInfo(), getFilesize(),
            isMsgHeaderFeatureFlagSet(TRUNCFILEMSG_FLAG_USE_QUOTA), getMsgHeaderUserID(),
            dynAttribs);
      if (truncRes == FhgfsOpsErr_SUCCESS && (shouldFixTimestamps() || getFileEvent()))
      {
         auto inode = metaStore->referenceFile(getEntryInfo());
         bool hasMissedEvents = true;
         if (inode)
         {
            if (shouldFixTimestamps())
               fixInodeTimestamp(*inode, mirroredTimestamps, nullptr);

            hasMissedEvents = inode->getNumHardlinks() > 1;

            metaStore->releaseFile(getEntryInfo()->getParentEntryID(), inode);
         }

         if (app->getFileEventLogger() && getFileEvent())
         {
               app->getFileEventLogger()->log(
                        *getFileEvent(),
                        getEntryInfo()->getEntryID(),
                        getEntryInfo()->getParentEntryID(),
                        "",
                        hasMissedEvents);
         }
      }
      return boost::make_unique<ResponseState>(truncRes);
   }

   MetaFileHandle inode = metaStore->referenceFile(getEntryInfo());
   if(!inode)
      return boost::make_unique<ResponseState>(FhgfsOpsErr_PATHNOTEXISTS);

   inode->setDynAttribs(dynAttribs);
   if (shouldFixTimestamps())
      fixInodeTimestamp(*inode, mirroredTimestamps, nullptr);
   inode->updateInodeOnDisk(getEntryInfo());

   metaStore->releaseFile(getEntryInfo()->getParentEntryID(), inode);

   return boost::make_unique<ResponseState>(FhgfsOpsErr_SUCCESS);
}

void TruncFileMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_TruncFileResp);
}
