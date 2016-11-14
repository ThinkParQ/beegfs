#include <common/app/log/LogContext.h>
#include <common/net/message/storage/quota/RequestExceededQuotaRespMsg.h>
#include "program/Program.h"

#include "RequestExceededQuotaMsgEx.h"


bool RequestExceededQuotaMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("RequestExceededQuotaMsgEx incoming");

   LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Received a RequestExceededQuotaMsgEx from: " +
      ctx.peerName() );

   App* app = Program::getApp();
   Config* cfg = app->getConfig();

   // get exceeded quota ID list
   RequestExceededQuotaRespMsg respMsg(getQuotaDataType(), getExceededType(), FhgfsOpsErr_INVAL);

   if(cfg->getQuotaEnableEnforcement() )
   {
      app->getQuotaManager()->getExceededQuotaStore()->getExceededQuota(
         respMsg.getExceededQuotaIDs(), getQuotaDataType(), getExceededType() );
      respMsg.setError(FhgfsOpsErr_SUCCESS);
   }
   else
   {
      log.log(Log_DEBUG, "Unable to provide exceeded quota IDs. "
         "The storage/metadata daemon on " + ctx.peerName() + " has quota enforcement enabled,"
         " but not this management daemon. "
         "Fix this configuration problem or quota enforcement will not work.");
   }

   ctx.sendResponse(respMsg);

   return true;
}
