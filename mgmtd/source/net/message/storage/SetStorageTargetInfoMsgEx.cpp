#include <common/net/message/storage/SetStorageTargetInfoRespMsg.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <program/Program.h>

#include "SetStorageTargetInfoMsgEx.h"

bool SetStorageTargetInfoMsgEx::processIncoming(ResponseContext& ctx)
{
   FhgfsOpsErr result;

   App* app = Program::getApp();

   if (app->isShuttingDown())
   {
      ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_TRYAGAIN, "Mgmtd shutting down."));
      return true;
   }

   InternodeSyncer* internodeSyncer = app->getInternodeSyncer();

   const StorageTargetInfoList& targetInfoList = getStorageTargetInfos();

   LOG_DEBUG("Set target info incoming", Log_DEBUG,
      "Number of reports: " + StringTk::uintToStr(targetInfoList.size() ) );

   NodeType nodeType = getNodeType();
   if (nodeType != NODETYPE_Storage && nodeType != NODETYPE_Meta)
      result = FhgfsOpsErr_INVAL;
   else
   {
      internodeSyncer->storeCapacityReports(targetInfoList, nodeType);
      result = FhgfsOpsErr_SUCCESS;
   }

   ctx.sendResponse(SetStorageTargetInfoRespMsg(result) );

   return true;
}
