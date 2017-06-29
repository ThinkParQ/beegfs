#include <common/net/message/storage/quota/SetDefaultQuotaRespMsg.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <common/storage/quota/QuotaData.h>
#include <components/quota/QuotaManager.h>
#include <program/Program.h>

#include "SetDefaultQuotaMsgEx.h"


bool SetDefaultQuotaMsgEx::processIncoming(ResponseContext& ctx)
{
   bool respVal = false;

   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   Logger* log = app->getLogger();

   if (app->isShuttingDown())
   {
      ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_TRYAGAIN, "Mgmtd shutting down."));
      return true;
   }

   if(cfg->getQuotaEnableEnforcement() )
   {
      QuotaManager* manager = app->getQuotaManager();

      if(type == QuotaDataType_USER)
         respVal = manager->updateDefaultUserLimits(inodes, size);
      else
         respVal = manager->updateDefaultGroupLimits(inodes, size);
   }
   else
   {
      log->log(Log_ERR, __FUNCTION__, "Unable to set default quota limits. Configuration problem "
         "detected. Quota enforcement is not enabled for this management daemon, but a admin tried "
         "to set default quota limits. Fix this configuration problem or quota enforcement with "
         "default quota limits will not work correctly.");
   }

   ctx.sendResponse(SetDefaultQuotaRespMsg(respVal) );

   return true;
}
