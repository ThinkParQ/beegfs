#include <common/net/message/nodes/GetTargetMappingsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "GetTargetMappingsMsgEx.h"


bool GetTargetMappingsMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();

   ctx.sendResponse(GetTargetMappingsRespMsg(app->getTargetMapper()->getMapping()));

   return true;
}

