#include <common/app/log/LogContext.h>
#include <common/net/message/storage/quota/SetExceededQuotaRespMsg.h>
#include "program/Program.h"

#include "SetExceededQuotaMsgEx.h"


bool SetExceededQuotaMsgEx::processIncoming(ResponseContext& ctx)
{
   bool retVal = true;
   FhgfsOpsErr errorCode = FhgfsOpsErr_SUCCESS;

   if(Program::getApp()->getConfig()->getQuotaEnableEnforcement() )
   {
      // get the storage pool for which quota is exceeded
      StoragePoolPtr storagePool =
         Program::getApp()->getStoragePoolStore()->getPool(getStoragePoolId());

      if (!storagePool)
      {
         LOG(QUOTA, WARNING, "Couldn't set exceeded quota, "
                      "because requested storage pool doesn't exist on storage server.",
                      ("storagePoolId", getStoragePoolId()));

         errorCode = FhgfsOpsErr_UNKNOWNPOOL;

         goto send_response;
      }

      // see if any of our targets belong to this pool and, if yes, set exceeded quota info
      for (const auto& mapping : Program::getApp()->getStorageTargets()->getTargets())
      {
         if (storagePool->hasTarget(mapping.first))
         {
            // update exceeded quota
            Program::getApp()->getExceededQuotaStores()->get(mapping.first)->
               updateExceededQuota(getExceededQuotaIDs(), getQuotaDataType(), getExceededType() );
         }
      }
   }
   else
   {
      LOG(QUOTA, ERR, "Unable to set exceeded quota IDs. Configuration problem detected. "
               "The management daemon on " + ctx.peerName() + " has quota enforcement enabled, "
               "but not this storage daemon. Fix this configuration problem or quota enforcement "
               "will not work correctly. If quota enforcement settings have changed recently in the "
               "mgmtd configuration, please restart all BeeGFS services.");

      errorCode = FhgfsOpsErr_INTERNAL;
   }

send_response:
   ctx.sendResponse(SetExceededQuotaRespMsg(errorCode) );

   return retVal;
}
