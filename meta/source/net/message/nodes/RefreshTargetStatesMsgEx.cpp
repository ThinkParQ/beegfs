#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>

#include "RefreshTargetStatesMsgEx.h"


bool RefreshTargetStatesMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("RefreshTargetStatesMsg incoming");

   App* app = Program::getApp();
   InternodeSyncer* syncer = app->getInternodeSyncer();

   // force update of target states
   syncer->setForceTargetStatesUpdate();

   // send response
   acknowledge(ctx);

   return true;
}

