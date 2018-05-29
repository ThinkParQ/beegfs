#include <common/net/message/storage/attribs/UpdateBacklinkRespMsg.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/MessagingTk.h>
#include <toolkit/StorageTkEx.h>
#include <program/Program.h>

#include "UpdateBacklinkMsgEx.h"

bool UpdateBacklinkMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "UpdateBacklinkMsgEx incoming";

   #ifdef BEEGFS_DEBUG
      LOG_DEBUG(logContext, Log_DEBUG, "Received a UpdateBacklinkMsg from: " + ctx.peerName() );
   #endif // BEEGFS_DEBUG

   App* app = Program::getApp();

   FhgfsOpsErr clientErrRes = FhgfsOpsErr_SUCCESS;

   // select the right targetID

   uint16_t targetID = getTargetID();

   if(isMsgHeaderFeatureFlagSet(UPDATEBACKLINKSMSG_FLAG_BUDDYMIRROR) )
   { // given targetID refers to a buddy mirror group
      MirrorBuddyGroupMapper* mirrorBuddies = app->getMirrorBuddyGroupMapper();

      targetID = isMsgHeaderFeatureFlagSet(UPDATEBACKLINKSMSG_FLAG_BUDDYMIRROR_SECOND) ?
         mirrorBuddies->getSecondaryTargetID(targetID) :
         mirrorBuddies->getPrimaryTargetID(targetID);

      // note: only log messe here, error handling will happen below through invalid targetFD
      if(unlikely(!targetID) )
         LogContext(logContext).logErr("Invalid mirror buddy group ID: " +
            StringTk::uintToStr(getTargetID() ) );
   }

   // get targetFD

   int targetFD = app->getTargetFD(targetID,
      isMsgHeaderFeatureFlagSet(UPDATEBACKLINKSMSG_FLAG_BUDDYMIRROR) );
   if(unlikely(targetFD == -1) )
   { // unknown targetID
      LogContext(logContext).logErr("Unknown targetID: " + StringTk::uintToStr(targetID) );
      clientErrRes = FhgfsOpsErr_UNKNOWNTARGET;
   }
   else
   { // valid targetID
      bool setRes = StorageTkEx::attachEntryInfoToChunk(targetFD, getPathInfo(), getEntryID(),
         getEntryInfoBuf(), getEntryInfoBufLen(), true);

      if(unlikely(!setRes) )
         clientErrRes = FhgfsOpsErr_INTERNAL;
   }

   ctx.sendResponse(UpdateBacklinkRespMsg(clientErrRes) );

   return true;
}
