#include "GetStoragePoolsMsgEx.h"

#include <common/net/message/nodes/storagepools/GetStoragePoolsRespMsg.h>
#include <program/Program.h>

bool GetStoragePoolsMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();
   StoragePoolStore* storagePoolStore = app->getStoragePoolStore();

   StoragePoolPtrVec poolsVec = storagePoolStore->getPoolsAsVec();

   ctx.sendResponse(GetStoragePoolsRespMsg(&poolsVec));
   
   return true;
}

