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
      app->getQuotaManager()->getDefaultLimits(storagePoolId, limits);
   }

   ctx.sendResponse(GetDefaultQuotaRespMsg(limits) );

   return true;
}
