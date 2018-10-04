#include <common/net/message/nodes/MapTargetsRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/ZipIterator.h>
#include <program/Program.h>

#include "MapTargetsMsgEx.h"


bool MapTargetsMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("MapTargetsMsg incoming");

   const App* app = Program::getApp();
   const NodeStoreServers* storageNodes = app->getStorageNodes();
   TargetMapper* targetMapper = app->getTargetMapper();

   const NumNodeID nodeID = getNodeID();
   std::map<uint16_t, FhgfsOpsErr> results;

   for (const auto mapping : getTargets())
   {
      const auto targetId = mapping.first;
      const auto poolId = mapping.second;

      const auto mapRes = targetMapper->mapTarget(targetId, nodeID, poolId);

      results[targetId] = mapRes.first;

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
      ctx.sendResponse(MapTargetsRespMsg(results));

   return true;
}

