#include <app/App.h>
#include <program/Program.h>
#include <common/net/message/storage/quota/GetQuotaInfoRespMsg.h>
#include <common/app/log/LogContext.h>
#include <common/storage/quota/GetQuotaConfig.h>
#include <common/toolkit/UnitTk.h>

// xfs/xfs.h includes linux/fs.h, which (sometimes) defines MS_RDONLY.
// sys/mount.h is the canonical source of MS_RDONLY, but declares it as an enum - which is then
// broken by linux/fs.h when included after it
#include <sys/mount.h>
#include <xfs/xfs.h>
#include <xfs/xqm.h>

#include "GetQuotaInfoMsgEx.h"


bool GetQuotaInfoMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();

   QuotaDataList outQuotaDataList;

   if(cfg->getQuotaEnableEnforcement() )
   {
      QuotaManager* quotaManager = app->getQuotaManager();

      if(getQueryType() == QUERY_TYPE_SINGLE_ID)
         requestQuotaForID(getIDRangeStart(), getType(), quotaManager, outQuotaDataList);
      else if(getQueryType() == QUERY_TYPE_ID_RANGE)
         requestQuotaForRange(quotaManager, outQuotaDataList);
      else if(getQueryType() == QUERY_TYPE_ID_LIST)
         requestQuotaForList(quotaManager, outQuotaDataList);
   }

   ctx.sendResponse(GetQuotaInfoRespMsg(&outQuotaDataList) );

   // do not report an error, also when no quota limits available
   return true;
}

/**
 * request the quota data for a single ID
 *
 * @param id the quota data of this ID are needed
 * @param type the type of the ID user or group
 * @param quotaManager the quota manager of the management daemon
 * @param outQuotaDataList the list with the updated quota data
 * @return true if quota data found for the ID
 */
bool GetQuotaInfoMsgEx::requestQuotaForID(unsigned id, QuotaDataType type,
   const QuotaManager* quotaManager, QuotaDataList& outQuotaDataList)
{
   QuotaData data(id, type);
   bool retVal = quotaManager->getQuotaLimitsForID(getStoragePoolId(), data);

   if(!retVal)
      data.setIsValid(false);

   outQuotaDataList.push_back(data);

   return retVal;
}

/**
 * request the quota data for a range
 *
 * @param quotaManager the quota manager of the management daemon
 * @param outQuotaDataList the list with the updated quota data
 * @return true if quota data found for the ID
 */
bool GetQuotaInfoMsgEx::requestQuotaForRange(const QuotaManager* quotaManager,
   QuotaDataList& outQuotaDataList)
{
   bool retVal = true;

   bool errorVal = quotaManager->getQuotaLimitsForRange(getIDRangeStart(), getIDRangeEnd(),
      (QuotaDataType)getType(), getStoragePoolId(), outQuotaDataList);

   if(!errorVal)
      retVal = false;

   return retVal;
}

/**
 * request the quota data for a list
 *
 * @param quotaManager the quota manager of the management daemon
 * @param outQuotaDataList the list with the updated quota data
 * @return true if quota data found for the ID
 */
bool GetQuotaInfoMsgEx::requestQuotaForList(const QuotaManager* quotaManager,
   QuotaDataList& outQuotaDataList)
{
   bool retVal = true;

   for(UIntListIter iter = getIDList()->begin(); iter != getIDList()->end(); iter++)
   {
      bool errorVal = requestQuotaForID(*iter, (QuotaDataType)this->getType(), quotaManager,
         outQuotaDataList);

      if(!errorVal)
         retVal = false;
   }

   return retVal;
}
