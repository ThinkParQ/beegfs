#include <common/net/message/session/locking/FLockAppendRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/SessionTk.h>
#include <net/msghelpers/MsgHelperLocking.h>
#include <program/Program.h>
#include <session/SessionStore.h>
#include <storage/MetaStore.h>
#include "FLockAppendMsgEx.h"


bool FLockAppendMsgEx::processIncoming(ResponseContext& ctx)
{
   /* note: this code is very similar to FLockRangeMsgEx::processIncoming(), so if you change
      something here, you probably want to change it there, too. */

#ifdef BEEGFS_DEBUG
   const char* logContext = "FLockAppendMsg incoming";
#endif // BEEGFS_DEBUG

   LOG_DEBUG(logContext, Log_DEBUG, "entryID: " + this->getEntryInfo()->getEntryID());

   clientResult = FhgfsOpsErr_INTERNAL;
   return BaseType::processIncoming(ctx);
}

FileIDLock FLockAppendMsgEx::lock(EntryLockStore& store)
{
   return {&store, getEntryInfo()->getEntryID(), true};
}

std::unique_ptr<MirroredMessageResponseState> FLockAppendMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   const char* logContext = "FLock Append Msg";

   unsigned ownerFD = SessionTk::ownerFDFromHandleID(getFileHandleID() );

   EntryLockDetails lockDetails(getClientNumID(), getClientFD(), getOwnerPID(), getLockAckID(),
      getLockTypeFlags() );

   LogContext(logContext).log(Log_DEBUG, lockDetails.toString());

   EntryInfo* entryInfo = getEntryInfo();
   clientResult = MsgHelperLocking::flockAppend(entryInfo, ownerFD, lockDetails);

   LogContext(logContext).log(Log_DEBUG, "FLock Append result: " + std::to_string(clientResult));

   Program::getApp()->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(),
      MetaOpCounter_FLOCKAPPEND, getMsgHeaderUserID() );

   return boost::make_unique<ResponseState>(clientResult);
}

void FLockAppendMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_FLockAppendResp, clientResult);
}
