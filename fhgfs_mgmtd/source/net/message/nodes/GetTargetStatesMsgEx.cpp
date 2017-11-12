#include <common/net/message/nodes/GetTargetStatesRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>

#include "GetTargetStatesMsgEx.h"


bool GetTargetStatesMsgEx::processIncoming(ResponseContext& ctx)
{
   LOG_DBG(STATES, DEBUG, "Received a GetTargetStatesMsg.", ctx.peerName() );

   App* app = Program::getApp();

   NodeType nodeType = getNodeType();

   TargetStateStore* targetStates;

   switch(nodeType)
   {
      case NODETYPE_Meta:
      {
         targetStates = app->getMetaStateStore();
      } break;

      case NODETYPE_Storage:
      {
         targetStates = app->getTargetStateStore();
      } break;

      default:
      {
         LOG(GENERAL, ERR, "Invalid node type.", nodeType);
         return false;
      } break;
   }


   UInt16List targetIDs;
   UInt8List reachabilityStates;
   UInt8List consistencyStates;

   targetStates->getStatesAsLists(targetIDs, reachabilityStates, consistencyStates);

   ctx.sendResponse(GetTargetStatesRespMsg(&targetIDs, &reachabilityStates, &consistencyStates) );

   return true;
}

