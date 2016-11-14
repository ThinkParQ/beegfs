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

   LOG_DEBUG(logContext, Log_DEBUG, "Received a FLockAppendMsg from: " + ctx.peerName() );
#endif // BEEGFS_DEBUG

   unsigned ownerFD = SessionTk::ownerFDFromHandleID(getFileHandleID() );

   EntryLockDetails lockDetails(getClientNumID(), getClientFD(), getOwnerPID(), getLockAckID(),
      getLockTypeFlags() );

   LOG_DEBUG(logContext, Log_SPAM, lockDetails.toString() );

   EntryInfo* entryInfo = getEntryInfo();

   FhgfsOpsErr clientResult = MsgHelperLocking::flockAppend(entryInfo, ownerFD, lockDetails);

   LOG_DEBUG(logContext, Log_SPAM, std::string("Client result: ") +
      FhgfsOpsErrTk::toErrString(clientResult) );

   ctx.sendResponse(FLockAppendRespMsg(clientResult) );

   Program::getApp()->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(),
      MetaOpCounter_FLOCKAPPEND, getMsgHeaderUserID() );

   return true;
}

