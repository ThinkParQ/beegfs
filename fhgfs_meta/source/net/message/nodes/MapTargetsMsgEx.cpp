#include <common/net/message/nodes/MapTargetsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "MapTargetsMsgEx.h"


bool MapTargetsMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("MapTargetsMsg incoming");

   LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Received a MapTargetsMsg from: " + ctx.peerName() );

   App* app = Program::getApp();
   NodeStoreServers* storageNodes = app->getStorageNodes();
   TargetMapper* targetMapper = app->getTargetMapper();

   NumNodeID nodeID = getNodeID();

   for(UInt16ListConstIter iter = getTargetIDs().begin(); iter != getTargetIDs().end(); iter++)
   {
      bool wasNewTarget = targetMapper->mapTarget(*iter, nodeID);
      if(wasNewTarget)
      {
         LOG_DEBUG_CONTEXT(log, Log_WARNING, "Mapping "
            "target " + StringTk::uintToStr(*iter) +
            " => " +
            storageNodes->getNodeIDWithTypeStr(nodeID) );

         IGNORE_UNUSED_VARIABLE(storageNodes);
      }
   }

   if(!acknowledge(ctx) )
      ctx.sendResponse(MapTargetsRespMsg(FhgfsOpsErr_SUCCESS) );

   return true;
}

