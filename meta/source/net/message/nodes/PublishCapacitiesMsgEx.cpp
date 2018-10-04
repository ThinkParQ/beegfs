#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>

#include "PublishCapacitiesMsgEx.h"


bool PublishCapacitiesMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("PublishCapacitiesMsg incoming");

   App* app = Program::getApp();
   InternodeSyncer* syncer = app->getInternodeSyncer();


   // force upload of capacity information
   syncer->setForcePublishCapacities();

   // send response
   acknowledge(ctx);

   return true;
}

