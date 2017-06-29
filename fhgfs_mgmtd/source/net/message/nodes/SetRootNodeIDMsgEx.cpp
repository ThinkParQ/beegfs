#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/nodes/SetRootNodeIDRespMsg.h>
#include <program/Program.h>

#include "SetRootNodeIDMsgEx.h"

bool SetRootNodeIDMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("SetRootNodeIDMsg incoming");

   LOG_DEBUG_CONTEXT(log, 4, "Received a SetRootNodeIDMsg from: " + ctx.peerName() );

   App* app = Program::getApp();

   if (app->isShuttingDown())
   {
      ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_TRYAGAIN, "Mgmtd shutting down."));
      return true;
   }

   NodeStoreServers* metaNodes = app->getMetaNodes();

   FhgfsOpsErr result;

   NumNodeID newRootNodeID(getRootNodeID() );
   uint16_t rootIsBuddyMirrored = getRootIsBuddyMirrored();
   
   bool setRes = metaNodes->setRootNodeNumID(newRootNodeID, true, rootIsBuddyMirrored);
   if(setRes)
      result = FhgfsOpsErr_SUCCESS;
   else
      result = FhgfsOpsErr_INTERNAL;

   ctx.sendResponse(SetRootNodeIDRespMsg(result) );

   return true;
}

