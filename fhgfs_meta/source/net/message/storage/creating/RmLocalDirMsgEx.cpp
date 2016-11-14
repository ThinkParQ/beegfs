#include <program/Program.h>
#include <common/net/message/storage/creating/RmLocalDirRespMsg.h>

#include "RmLocalDirMsgEx.h"


DirIDLock RmLocalDirMsgEx::lock(EntryLockStore& store)
{
   return {&store, getDelEntryInfo()->getEntryID(), true};
}

bool RmLocalDirMsgEx::processIncoming(ResponseContext& ctx)
{
#ifdef BEEGFS_DEBUG
   const char* logContext = "RmLocalDirMsg incoming";

   LOG_DEBUG(logContext, 4, "Received a RmLocalDirMsg from: " + ctx.peerName() );
#endif // BEEGFS_DEBUG

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

bool RmLocalDirMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   return sendToSecondary(ctx, *this, NETMSGTYPE_RmLocalDirResp) == FhgfsOpsErr_SUCCESS;
}
