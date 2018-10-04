#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/nodes/RegisterTargetRespMsg.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "RegisterTargetMsgEx.h"


bool RegisterTargetMsgEx::processIncoming(ResponseContext& ctx)
{
   const App* app = Program::getApp();

   if (app->isShuttingDown())
   {
      ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_TRYAGAIN, "Mgmtd shutting down."));
      return true;
   }

   const Config* cfg = app->getConfig();
   NumericIDMapper* targetNumIDMapper = app->getTargetNumIDMapper();

   const std::string targetID = getTargetID();
   const uint16_t targetNumID = getTargetNumID();

   LOG_DBG(GENERAL, DEBUG, "", targetID, targetNumID);

   // look if the target already exists in IDMapper
   const bool targetExists = (targetNumIDMapper->getNumericID(targetID) != 0);

   if (!cfg->getSysAllowNewTargets() && !targetExists)
   { // target doesn't exist, but no new targets allowed => reject
      // target is new => reject
      LOG(GENERAL, WARNING, "Registration of new targets disabled.",
         ("candidate target", targetID),
         ("from", ctx.peerName()));

      ctx.sendResponse(RegisterTargetRespMsg(0)); // send response with 0 as new target ID
      return true;
   }

   uint16_t newTargetNumID;
   const bool targetWasMapped =
         targetNumIDMapper->mapStringID(targetID, targetNumID, &newTargetNumID);

   if(!targetWasMapped)
   { // registration failed
      LOG(GENERAL, CRITICAL, "Registration failed.", targetID, targetNumID);

      newTargetNumID = 0;
   }
   else
   { // registration successful
      if(!targetExists) // new target registration
         LOG(GENERAL, NOTICE, "New target registered.", targetID, newTargetNumID);
   }

   ctx.sendResponse(RegisterTargetRespMsg(newTargetNumID) );

   return true;
}

