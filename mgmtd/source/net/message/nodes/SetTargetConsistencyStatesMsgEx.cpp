#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesRespMsg.h>
#include <nodes/MgmtdTargetStateStore.h>
#include <program/Program.h>

#include "SetTargetConsistencyStatesMsgEx.h"

bool SetTargetConsistencyStatesMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();

   if (app->isShuttingDown())
   {
      ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_TRYAGAIN, "Mgmtd shutting down."));
      return true;
   }

   MgmtdTargetStateStore* stateStore;

   const NodeType nodeType = getNodeType();

   switch (nodeType)
   {
      case NODETYPE_Storage:
         stateStore = app->getTargetStateStore();
         break;
      case NODETYPE_Meta:
         stateStore = app->getMetaStateStore();
         break;
      default:
         LOG(STATES, ERR, "Invalid node type.", nodeType, ("Sender", ctx.peerName()));
         return false;
   }

   bool setOnline = getSetOnline();

   stateStore->setConsistencyStatesFromLists(getTargetIDs(), getStates(), setOnline);

   FhgfsOpsErr result = FhgfsOpsErr_SUCCESS;

   ctx.sendResponse(SetTargetConsistencyStatesRespMsg(result) );

   stateStore->saveStatesToFile();

   return true;
}
