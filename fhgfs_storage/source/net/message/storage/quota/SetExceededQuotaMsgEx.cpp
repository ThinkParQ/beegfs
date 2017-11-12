#include <common/app/log/LogContext.h>
#include <common/net/message/storage/quota/SetExceededQuotaRespMsg.h>
#include "program/Program.h"

#include "SetExceededQuotaMsgEx.h"


bool SetExceededQuotaMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("SetExceededQuotaMsgEx incoming");

   LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Received a SetExceededQuotaMsgEx from: " + ctx.peerName() );

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
                      as("storagePoolId", getStoragePoolId()));

         errorCode = FhgfsOpsErr_UNKNOWNPOOL;

         goto send_response;
      }

      // see if any of our targets belong to this pool and, if yes, set exceeded quota info
      UInt16List targetIdList;
      Program::getApp()->getStorageTargets()->getAllTargetIDs(&targetIdList);

      for (auto iter = targetIdList.begin(); iter != targetIdList.end(); iter++)
      {
         if (storagePool->hasTarget(*iter))
         {
            // update exceeded quota
            Program::getApp()->getExceededQuotaStores()->get(*iter)->
               updateExceededQuota(getExceededQuotaIDs(), getQuotaDataType(), getExceededType() );
         }
      }
   }
   else
   {
      LOG(QUOTA, ERR, "Unable to set exceeded quota IDs. Configuration problem detected. "
               "The management daemon on " + ctx.peerName() + " has quota enforcement enabled, "
               "but not this storage daemon. Fix this configuration problem or quota enforcement "
               "willnot work correctly.");

      errorCode = FhgfsOpsErr_INTERNAL;
   }

send_response:
   ctx.sendResponse(SetExceededQuotaRespMsg(errorCode) );

   return retVal;
}
