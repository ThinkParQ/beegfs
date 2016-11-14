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
      // update exceeded quota
      Program::getApp()->getExceededQuotaStore()->updateExceededQuota(getExceededQuotaIDs(),
         this->getQuotaDataType(), this->getExceededType() );
   }
   else
   {
      log.log(Log_ERR, "Unable to set exceeded quota IDs. Configuration problem detected. "
         "The management daemon on " + ctx.peerName() + " has quota enforcement enabled, "
         "but not this storage daemon. Fix this configuration problem or quota enforcement will "
         "not work correctly.");

      errorCode = FhgfsOpsErr_INTERNAL;
   }

   ctx.sendResponse(SetExceededQuotaRespMsg(errorCode) );

   return retVal;
}
