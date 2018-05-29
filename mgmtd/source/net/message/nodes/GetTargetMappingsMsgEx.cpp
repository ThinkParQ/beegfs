#include <common/net/message/nodes/GetTargetMappingsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "GetTargetMappingsMsgEx.h"


bool GetTargetMappingsMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("GetTargetMappingsMsg incoming");

   LOG_DEBUG_CONTEXT(log, 4, "Received a GetTargetMappingsMsg from: " + ctx.peerName() );

   App* app = Program::getApp();
   TargetMapper* targetMapper = app->getTargetMapper();

   UInt16List targetIDs;
   NumNodeIDList nodeIDs;

   targetMapper->getMappingAsLists(targetIDs, nodeIDs);

   ctx.sendResponse(GetTargetMappingsRespMsg(&targetIDs, &nodeIDs) );

   return true;
}

