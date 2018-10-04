#include "RemoveStoragePoolMsgEx.h"

#include <common/app/log/LogContext.h>
#include <common/net/message/nodes/storagepools/RemoveStoragePoolRespMsg.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <common/nodes/StoragePoolStore.h>
#include <program/Program.h>

bool RemoveStoragePoolMsgEx::processIncoming(ResponseContext& ctx)
{
   if (Program::getApp()->isShuttingDown())
   {
      ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_TRYAGAIN, "Mgmtd shutting down."));
      return true;
   }

   StoragePoolStoreEx* storagePoolStore = Program::getApp()->getStoragePoolStore();

   if (Program::getApp()->isShuttingDown())
   {
      ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_TRYAGAIN, "Mgmtd shutting down."));
      return true;
   }

   FhgfsOpsErr rmRes = storagePoolStore->removePool(poolId);

   if (rmRes == FhgfsOpsErr_SUCCESS)
      Program::getApp()->getHeartbeatMgr()->notifyAsyncRefreshStoragePools();

   ctx.sendResponse(RemoveStoragePoolRespMsg(rmRes));

   return true;
}

