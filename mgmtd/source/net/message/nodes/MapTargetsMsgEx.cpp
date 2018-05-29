#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/nodes/MapTargetsRespMsg.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/ZipIterator.h>
#include <program/Program.h>
#include "MapTargetsMsgEx.h"


bool MapTargetsMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("MapTargetsMsg incoming");

   LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Received a MapTargetsMsg from: " + ctx.peerName() );

   const App* app = Program::getApp();
   const Config* cfg = app->getConfig();
   const NodeStoreServers* storageNodes = app->getStorageNodes();
   TargetMapper* targetMapper = app->getTargetMapper();
   InternodeSyncer* syncer = app->getInternodeSyncer();

   const NumNodeID nodeID = getNodeID();
   const auto targetVec = getTargetVec();
   FhgfsOpsErrVec responseCodeVec;

   if (app->isShuttingDown())
   {
      ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_TRYAGAIN, "Mgmdt shutting down."));
      return true;
   }

   bool isNodeActive = storageNodes->isNodeActive(nodeID);
   if(unlikely(!isNodeActive) )
   { // santiy check: unknown nodeID here should never happen
      LOG(GENERAL, ERR, "Refusing to map targets to unknown storage server.", nodeID);

      responseCodeVec.resize(targetVec.size(), FhgfsOpsErr_UNKNOWNNODE); // => error is the same
                                                                         //    for all targets
      goto send_response;
   }

   // walk over all targets and map them to the given nodeID
   for (auto iter = targetVec.begin(); iter != targetVec.end(); iter++)
   { // => targetVec is a vector with "targetID,storagePoolID" as elements
      const auto targetId = iter->first;
      const auto poolId = iter->second;

      if(unlikely(targetId == 0) )
      { // santiy check: undefined targetNumID here should never happen
         LOG(GENERAL, ERR, "Refusing to map target with undefined numeric ID to node.", nodeID);

         responseCodeVec.push_back(FhgfsOpsErr_UNKNOWNTARGET);
         continue;
      }

      if(!cfg->getSysAllowNewTargets() )
      { // no new targets allowed => check if target is new
         const NumNodeID existingNodeID = targetMapper->getNodeID(targetId);
         if(existingNodeID == 0)
         { // target is new => reject
            LOG(GENERAL, WARNING, "Registration of new targets disabled.",
               as("candidate target", targetId),
               as("from", ctx.peerName()));

            responseCodeVec.push_back(FhgfsOpsErr_UNKNOWNTARGET);
            continue;
         }
      }

      const auto mapRes = targetMapper->mapTarget(targetId, nodeID, poolId);

      responseCodeVec.push_back(mapRes.first);

      if ( (mapRes.first == FhgfsOpsErr_SUCCESS) && (mapRes.second) )
      { // target was mapped and is new
         LOG_DBG(GENERAL, WARNING, "Mapping target.", targetId,
               as("node", storageNodes->getNodeIDWithTypeStr(nodeID)));

         // async notification of other nodes (one target at a time to keep UDP msgs small)
         app->getHeartbeatMgr()->notifyAsyncAddedTarget(targetId, nodeID, poolId);
      }
   }

   // force update of capacity pools
   /* (because maybe server was restarted and thus might have been in the emergency pool while the
      target was unreachable) */
   syncer->setForcePoolsUpdate();

send_response:
   if(!acknowledge(ctx) )
      ctx.sendResponse(MapTargetsRespMsg(&responseCodeVec));

   return true;
}

