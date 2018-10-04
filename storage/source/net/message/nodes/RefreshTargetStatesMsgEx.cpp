#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>

#include "RefreshTargetStatesMsgEx.h"


bool RefreshTargetStatesMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();
   InternodeSyncer* syncer = app->getInternodeSyncer();


   // force update of capacity pools
   syncer->setForceTargetStatesUpdate();

   // send response
   acknowledge(ctx);

   return true;
}

