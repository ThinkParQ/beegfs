#include <common/net/message/nodes/GetTargetMappingsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "GetTargetMappingsMsgEx.h"


bool GetTargetMappingsMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();
   TargetMapper* targetMapper = app->getTargetMapper();

   ctx.sendResponse(GetTargetMappingsRespMsg(targetMapper->getMapping()));

   return true;
}

