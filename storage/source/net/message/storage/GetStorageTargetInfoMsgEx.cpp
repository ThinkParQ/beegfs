#include <common/net/message/storage/GetStorageTargetInfoRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "GetStorageTargetInfoMsgEx.h"


bool GetStorageTargetInfoMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "GetStorageTargetInfoMsg incoming";

   App* app = Program::getApp();
   StorageTargets* storageTargets = app->getStorageTargets();

   LOG_DEBUG(logContext, Log_DEBUG, "Received a GetStorageTargetInfoMsg from: " + ctx.peerName() );
   IGNORE_UNUSED_VARIABLE(logContext);

   StorageTargetInfoList targetInfoList;

   if(getTargetIDs().empty() )
   {
      UInt16List targetIDs;
      storageTargets->getAllTargetIDs(&targetIDs);
      storageTargets->generateTargetInfoList(targetIDs, targetInfoList);
   }
   else
      storageTargets->generateTargetInfoList(getTargetIDs(), targetInfoList);

   ctx.sendResponse(GetStorageTargetInfoRespMsg(&targetInfoList) );

   // update stats

   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(),
      StorageOpCounter_STATSTORAGEPATH, getMsgHeaderUserID() );

   return true;
}

