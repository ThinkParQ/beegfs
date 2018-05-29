#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>

#include "RefreshTargetStatesMsgEx.h"


bool RefreshTargetStatesMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("RefreshTargetStatesMsg incoming");

   LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Received a RefreshTargetStatesMsg from: " + ctx.peerName() );

   App* app = Program::getApp();
   InternodeSyncer* syncer = app->getInternodeSyncer();

   // force update of target states
   syncer->setForceTargetStatesUpdate();

   // send response
   acknowledge(ctx);

   return true;
}

