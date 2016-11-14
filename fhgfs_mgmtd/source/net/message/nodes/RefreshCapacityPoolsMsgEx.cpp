#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "RefreshCapacityPoolsMsgEx.h"


bool RefreshCapacityPoolsMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("RefreshCapacityPoolsMsg incoming");

   LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Received a RefreshCapacityPoolsMsg from: " + ctx.peerName() );

   App* app = Program::getApp();
   InternodeSyncer* syncer = app->getInternodeSyncer();


   // force update of capacity pools
   syncer->setForcePoolsUpdate();


   // send response
   acknowledge(ctx);

   return true;
}

