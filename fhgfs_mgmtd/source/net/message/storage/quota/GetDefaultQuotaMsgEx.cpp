#include <app/config/Config.h>
#include <common/net/message/storage/quota/GetDefaultQuotaRespMsg.h>
#include <components/quota/QuotaManager.h>
#include <program/Program.h>
#include "GetDefaultQuotaMsgEx.h"



bool GetDefaultQuotaMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();

   QuotaDefaultLimits limits;

   if(cfg->getQuotaEnableEnforcement() )
   {
      QuotaManager* manager = app->getQuotaManager();
      limits = QuotaDefaultLimits(manager->getDefaultLimits() );
   }

   ctx.sendResponse(GetDefaultQuotaRespMsg(limits) );

   return true;
}
