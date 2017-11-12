#include <common/net/message/nodes/MapTargetsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/ZipIterator.h>
#include <program/Program.h>

#include "MapTargetsMsgEx.h"


bool MapTargetsMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("MapTargetsMsg incoming");

   LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Received a MapTargetsMsg from: " + ctx.peerName() );

   const App* app = Program::getApp();
   const NodeStoreServers* storageNodes = app->getStorageNodes();
   TargetMapper* targetMapper = app->getTargetMapper();

   const NumNodeID nodeID = getNodeID();
   const auto targetVec = getTargetVec();
   FhgfsOpsErrVec responseCodeVec;

   for (auto iter = targetVec.begin(); iter != targetVec.end(); iter++)
   { // => targetVec is a vector with "targetID,storagePoolID" as elements
      const auto targetId = iter->first;
      const auto poolId = iter->second;

      const auto mapRes = targetMapper->mapTarget(targetId, nodeID, poolId);

      responseCodeVec.push_back(mapRes.first);

      if ( (mapRes.first != FhgfsOpsErr_SUCCESS) && (mapRes.second) )
      { // target could be mapped and is new
         LOG_DEBUG_CONTEXT(log, Log_WARNING, "Mapping "
            "target " + StringTk::uintToStr(targetId) +
            " => " +
            storageNodes->getNodeIDWithTypeStr(nodeID) );

         IGNORE_UNUSED_VARIABLE(storageNodes);
      }
   }

   if(!acknowledge(ctx) )
      ctx.sendResponse(MapTargetsRespMsg(&responseCodeVec) );

   return true;
}

