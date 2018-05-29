#include <app/App.h>
#include <common/app/log/LogContext.h>
#include <common/net/message/storage/quota/GetQuotaInfoRespMsg.h>
#include <common/storage/quota/GetQuotaConfig.h>
#include <common/storage/quota/Quota.h>
#include <session/ZfsSession.h>
#include <storage/QuotaBlockDevice.h>
#include <program/Program.h>
#include <toolkit/QuotaTk.h>
#include "GetQuotaInfoMsgEx.h"



bool GetQuotaInfoMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "GetQuotaInfo (GetQuotaInfoMsg incoming)";

   LOG_DEBUG(logContext, Log_DEBUG, "Received a GetQuotaInfoMsg from: " + ctx.peerName() );

   App *app = Program::getApp();

   QuotaBlockDeviceMap quotaBlockDevices;
   QuotaDataList outQuotaDataList;
   ZfsSession session;
   QuotaInodeSupport quotaInodeSupport = QuotaInodeSupport_UNKNOWN;

   switch(getTargetSelection() )
   {
      case GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST:
         app->getStorageTargets()->getQuotaBlockDevices(&quotaBlockDevices);
         quotaInodeSupport = app->getStorageTargets()->getSupportForInodeQuota();
         break;
      case GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST_PER_TARGET:
         app->getStorageTargets()->getQuotaBlockDevice(&quotaBlockDevices, getTargetNumID(),
            &quotaInodeSupport);
         break;
      case GETQUOTACONFIG_SINGLE_TARGET:
         app->getStorageTargets()->getQuotaBlockDevice(&quotaBlockDevices, getTargetNumID(),
            &quotaInodeSupport);
         break;
   }

   if(quotaBlockDevices.empty() )
      /* no quota data available but do not return an error during message processing, it's not
      the correct place for error handling in this case */
      LogContext(logContext).logErr("Error: no quota block devices.");
   else
   {
      if(getQueryType() == GetQuotaInfo_QUERY_TYPE_SINGLE_ID)
         QuotaTk::appendQuotaForID(getIDRangeStart(), getType(), &quotaBlockDevices,
            &outQuotaDataList, &session);
      else
      if(getQueryType() == GetQuotaInfo_QUERY_TYPE_ID_RANGE)
         QuotaTk::requestQuotaForRange(&quotaBlockDevices, getIDRangeStart(),
            getIDRangeEnd(), getType(), &outQuotaDataList, &session);
      else
      if(getQueryType() == GetQuotaInfo_QUERY_TYPE_ID_LIST)
      {
         QuotaTk::requestQuotaForList(&quotaBlockDevices, getIDList(), getType(),
            &outQuotaDataList, &session);
      }
   }

   // send response
   ctx.sendResponse(GetQuotaInfoRespMsg(&outQuotaDataList, quotaInodeSupport) );

   return true;
}
