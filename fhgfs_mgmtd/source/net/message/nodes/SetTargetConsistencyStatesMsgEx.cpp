#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/nodes/SetTargetConsistencyStatesRespMsg.h>
#include <nodes/MgmtdTargetStateStore.h>
#include <program/Program.h>

#include "SetTargetConsistencyStatesMsgEx.h"

bool SetTargetConsistencyStatesMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "Set target states incoming";
   LOG_DEBUG(logContext, Log_DEBUG, "Received a SetTargetConsistencyStatesMsg from: "
      + ctx.peerName() );

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
         LogContext(logContext).logErr("Invalid node type: " + StringTk::intToStr(nodeType) );
         return false;
   }

   bool setOnline = getSetOnline();

   stateStore->setConsistencyStatesFromLists(getTargetIDs(), getStates(), setOnline);

   FhgfsOpsErr result = FhgfsOpsErr_SUCCESS;

   ctx.sendResponse(SetTargetConsistencyStatesRespMsg(result) );

   stateStore->saveStatesToFile();

   return true;
}
