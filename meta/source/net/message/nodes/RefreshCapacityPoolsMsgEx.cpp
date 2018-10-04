#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "RefreshCapacityPoolsMsgEx.h"


bool RefreshCapacityPoolsMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("RefreshCapacityPoolsMsg incoming");

   App* app = Program::getApp();
   InternodeSyncer* syncer = app->getInternodeSyncer();


   // force update of capacity pools
   syncer->setForcePoolsUpdate();


   // send response
   acknowledge(ctx);

   return true;
}

