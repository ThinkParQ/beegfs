#include <common/app/log/LogContext.h>
#include <common/net/message/storage/quota/RequestExceededQuotaRespMsg.h>
#include <nodes/StoragePoolStoreEx.h>
#include <storage/StoragePoolEx.h>

#include <program/Program.h>

#include "RequestExceededQuotaMsgEx.h"


bool RequestExceededQuotaMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("RequestExceededQuotaMsgEx incoming");

   App* app = Program::getApp();
   Config* cfg = app->getConfig();

   // get exceeded quota ID list
   RequestExceededQuotaRespMsg respMsg(getQuotaDataType(), getExceededType(), FhgfsOpsErr_NOTSUPP);

   if(cfg->getQuotaEnableEnforcement() )
   {
      // if storagePoolId is set (i.e. != INVALID) take that pool; otherwise try to take a given
      // targetID and get the appropriate pool (i.e. the pool which has the target)
      StoragePoolExPtr storagePool;
      StoragePoolId poolId = getStoragePoolId();
      if (poolId != StoragePoolStore::INVALID_POOL_ID)
         storagePool = app->getStoragePoolStore()->getPool(poolId);
      else
      {
         storagePool = app->getStoragePoolStore()->getPool(getTargetId());
      }

      if (storagePool)
      {
         storagePool->getExceededQuotaStore()->getExceededQuota(
            respMsg.getExceededQuotaIDs(), getQuotaDataType(), getExceededType() );
         respMsg.setError(FhgfsOpsErr_SUCCESS);
      }
      else
      {
         respMsg.setError(FhgfsOpsErr_INVAL);
      }
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
