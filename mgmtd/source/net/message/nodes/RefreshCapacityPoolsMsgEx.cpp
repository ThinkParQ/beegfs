#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "RefreshCapacityPoolsMsgEx.h"


bool RefreshCapacityPoolsMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();

   if (app->isShuttingDown())
      return true;

   InternodeSyncer* syncer = app->getInternodeSyncer();


   // force update of capacity pools
   syncer->setForcePoolsUpdate();


   // send response
   acknowledge(ctx);

   return true;
}

