#include <common/app/log/LogContext.h>
#include <common/Common.h>
#include <common/net/message/storage/quota/SetQuotaRespMsg.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <program/Program.h>

#include "SetQuotaMsgEx.h"



bool SetQuotaMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("SetQuotaMsg incoming");

   LOG_DEBUG_CONTEXT(log, 4, "Received a SetQuotaMsg from: " + ctx.peerName() );

   if (Program::getApp()->isShuttingDown())
   {
      ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_TRYAGAIN, "Mgmtd shutting down."));
      return true;
   }

   if (Program::getApp()->isShuttingDown())
   {
      ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_TRYAGAIN, "Mgmtd shutting down."));
      return true;
   }

   int respVal = processQuotaLimits();

   ctx.sendResponse(SetQuotaRespMsg(respVal) );

   return respVal;
}

/**
 * updates the quota limits
 */
bool SetQuotaMsgEx::processQuotaLimits()
{
   LogContext log("SetQuotaMsg process");
   App* app = Program::getApp();

   if(app->getConfig()->getQuotaEnableEnforcement() )
      app->getQuotaManager()->updateQuotaLimits(getStoragePoolId(), getQuotaDataList() );
   else
      LOG(QUOTA, ERR, "Unable to set quota limits. Configuration problem detected: Quota "
         "enforcement is not enabled for this management daemon, but an admin tried to set quota "
         "limits. The configuration must be updated for quota enforcement to work correctly.");

   return true;
}
