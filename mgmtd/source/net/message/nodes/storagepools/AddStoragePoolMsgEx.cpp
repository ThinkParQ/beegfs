#include "AddStoragePoolMsgEx.h"

#include <common/app/log/LogContext.h>
#include <common/net/message/nodes/storagepools/AddStoragePoolRespMsg.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <common/nodes/StoragePoolStore.h>
#include <program/Program.h>

bool AddStoragePoolMsgEx::processIncoming(ResponseContext& ctx)
{
   StoragePoolStore* storagePoolStore = Program::getApp()->getStoragePoolStore();

   if (Program::getApp()->isShuttingDown())
   {
      ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_TRYAGAIN, "Mgmtd shutting down."));
      return true;
   }

   if (Program::getApp()->isShuttingDown())
   {
      ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_TRYAGAIN, "Mgmtd shutting down."));
      return true;
   }

   std::pair<FhgfsOpsErr, StoragePoolId> createRes =
         storagePoolStore->createPool(poolId, description, &targets, &buddyGroups);

   // FhgfsOpsErr_INVAL means that at least one target or buddy group could not be moved to the new
   // pool
   if (createRes.first == FhgfsOpsErr_SUCCESS || createRes.first == FhgfsOpsErr_INVAL)
      Program::getApp()->getHeartbeatMgr()->notifyAsyncRefreshStoragePools();

   ctx.sendResponse(AddStoragePoolRespMsg(createRes.first, createRes.second));

   return true;
}

