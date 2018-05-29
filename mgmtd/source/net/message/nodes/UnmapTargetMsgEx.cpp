#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/nodes/UnmapTargetRespMsg.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "UnmapTargetMsgEx.h"


bool UnmapTargetMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("UnmapTargetMsg incoming");

   LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Received a UnmapTargetMsg from: " + ctx.peerName() );

   App* app = Program::getApp();

   if (app->isShuttingDown())
   {
      ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_TRYAGAIN, "Mgmtd shutting down."));
      return true;
   }

   TargetMapper* targetMapper = app->getTargetMapper();

   uint16_t targetID = getTargetID();

   LOG_DBG(GENERAL, WARNING, "Unmapping target.", targetID);

   bool targetExisted = targetMapper->unmapTarget(targetID);

   if(app->getConfig()->getQuotaEnableEnforcement() )
      app->getQuotaManager()->removeTargetFromQuotaDataStores(targetID);

   if(targetExisted)
   {
      // force update of capacity pools (especially to update buddy mirror capacity pool)
      app->getInternodeSyncer()->setForcePoolsUpdate();
   }

   ctx.sendResponse(
         UnmapTargetRespMsg(targetExisted ? FhgfsOpsErr_SUCCESS : FhgfsOpsErr_UNKNOWNTARGET) );

   return true;
}

