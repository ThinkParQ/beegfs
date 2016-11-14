#include <common/net/message/nodes/RegisterTargetRespMsg.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "RegisterTargetMsgEx.h"


bool RegisterTargetMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("RegisterTargetMsg incoming");

   LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Received a RegisterTargetMsg from: " + ctx.peerName() );

   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   NumericIDMapper* targetNumIDMapper = app->getTargetNumIDMapper();

   std::string targetID = getTargetID();
   uint16_t targetNumID = getTargetNumID();

   uint16_t newTargetNumID;
   bool targetWasMapped;

   LOG_DEBUG_CONTEXT(log, Log_DEBUG,
      "Target: " + targetID + "; "
      "numID: " + StringTk::uintToStr(targetNumID) );

   if(!cfg->getSysAllowNewTargets() )
   { // no new targets allowed => check if target is new
      uint16_t existingNumID = targetNumIDMapper->getNumericID(targetID);
      if(!existingNumID)
      { // target is new => reject
         log.log(Log_WARNING, "Registration of new targets disabled. "
            "Rejecting storage target: " + targetID + " "
            "(from: " + ctx.peerName() + ")");

         newTargetNumID = 0;
         goto send_response;
      }
   }

   targetWasMapped = targetNumIDMapper->mapStringID(targetID, targetNumID, &newTargetNumID);

   if(!targetWasMapped)
   { // registration failed
      log.log(Log_CRITICAL, "Registration failed for target: " + targetID + "; "
         "numID: " + StringTk::uintToStr(targetNumID) );

      newTargetNumID = 0;
   }
   else
   { // registration successful
      if(!targetNumID) // new target registration
         log.log(Log_NOTICE, "New target registered: " + targetID + "; "
            "numID: " + StringTk::uintToStr(newTargetNumID) );
   }

send_response:
   ctx.sendResponse(RegisterTargetRespMsg(newTargetNumID) );

   return true;
}

