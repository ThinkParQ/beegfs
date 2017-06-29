#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/nodes/MapTargetsRespMsg.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "MapTargetsMsgEx.h"


bool MapTargetsMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("MapTargetsMsg incoming");

   LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Received a MapTargetsMsg from: " + ctx.peerName() );

   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   NodeStoreServers* storageNodes = app->getStorageNodes();
   TargetMapper* targetMapper = app->getTargetMapper();
   InternodeSyncer* syncer = app->getInternodeSyncer();

   NumNodeID nodeID = getNodeID();

   if (app->isShuttingDown())
   {
      ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_TRYAGAIN, "Mgmdt shutting down."));
      return true;
   }

   FhgfsOpsErr responseError = FhgfsOpsErr_SUCCESS;

   //LOG_DEBUG_CONTEXT(log, Log_WARNING, "Mapping " +
   //   StringTk::uintToStr(targetIDs.size() ) + " targets "
   //   "to node: " + storageNodes->getNodeIDWithTypeStr(nodeID) ); // debug in

   bool isNodeActive = storageNodes->isNodeActive(nodeID);
   if(unlikely(!isNodeActive) )
   { // santiy check: unknown nodeID here should never happen
      log.logErr("Refusing to map targets to unknown storage server ID: " + nodeID.str());

      responseError = FhgfsOpsErr_UNKNOWNNODE;
      goto send_response;
   }

   // walk over all targets and map them to the given nodeID

   for(UInt16ListConstIter iter = getTargetIDs().begin(); iter != getTargetIDs().end(); iter++)
   {
      if(unlikely(!*iter) )
      { // santiy check: undefined targetNumID here should never happen
         log.logErr("Refusing to map target with undefined numeric ID to node: " + nodeID.str() );

         responseError = FhgfsOpsErr_UNKNOWNTARGET;
         continue;
      }

      if(!cfg->getSysAllowNewTargets() )
      { // no new targets allowed => check if target is new
         NumNodeID existingNodeID = targetMapper->getNodeID(*iter);
         if(existingNodeID == 0)
         { // target is new => reject
            log.log(Log_WARNING, "Registration of new targets disabled. "
               "Rejecting storage target: " + StringTk::uintToStr(*iter) + " "
               "(from: " + ctx.peerName() + ")");

            responseError = FhgfsOpsErr_UNKNOWNTARGET;
            continue;
         }
      }


      bool wasNewTarget = targetMapper->mapTarget(*iter, nodeID);
      if(wasNewTarget)
      {
         LOG_DEBUG_CONTEXT(log, Log_WARNING, "Mapping "
            "target " + StringTk::uintToStr(*iter) +
            " => " +
            storageNodes->getNodeIDWithTypeStr(nodeID) );

         // async notification of other nodes (one target at a time to keep UDP msgs small)
         app->getHeartbeatMgr()->notifyAsyncAddedTarget(*iter, nodeID);
      }
   }

   // force update of capacity pools
   /* (because maybe server was restarted and thus might have been in the emergency pool while the
      target was unreachable) */
   syncer->setForcePoolsUpdate();

send_response:
   if(!acknowledge(ctx) )
      ctx.sendResponse(MapTargetsRespMsg(responseError) );

   return true;
}

