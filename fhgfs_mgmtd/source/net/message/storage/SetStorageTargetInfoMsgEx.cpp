#include <common/net/message/storage/SetStorageTargetInfoRespMsg.h>
#include <program/Program.h>

#include "SetStorageTargetInfoMsgEx.h"

bool SetStorageTargetInfoMsgEx::processIncoming(ResponseContext& ctx)
{
   LOG_DEBUG("Set target info incoming", Log_DEBUG,
      "Received a SetStorageTargetInfoMsg from: " + ctx.peerName() );

   FhgfsOpsErr result;

   App* app = Program::getApp();
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
