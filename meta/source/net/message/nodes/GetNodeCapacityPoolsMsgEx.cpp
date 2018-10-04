#include <common/net/message/nodes/GetNodeCapacityPoolsRespMsg.h>
#include <common/storage/StoragePool.h>
#include <program/Program.h>

#include "GetNodeCapacityPoolsMsgEx.h"

bool GetNodeCapacityPoolsMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "GetNodeCapacityPools incoming";

   CapacityPoolQueryType poolType = getCapacityPoolQueryType();

   LOG_DEBUG(logContext, Log_SPAM, "PoolType: " + StringTk::intToStr(poolType) );

   const App* app = Program::getApp();

   GetNodeCapacityPoolsRespMsg::PoolsMap capacityPoolsMap;

   switch(poolType)
   {
      case CapacityPoolQuery_META:
      {
         const NodeCapacityPools* capPools = app->getMetaCapacityPools();

         capacityPoolsMap[StoragePoolId(StoragePoolStore::INVALID_POOL_ID)] =
               capPools->getPoolsAsLists();
      } break;

      case CapacityPoolQuery_STORAGE:
      {
         const StoragePoolPtrVec storagePools = app->getStoragePoolStore()->getPoolsAsVec();

         for (auto iter = storagePools.begin(); iter != storagePools.end(); iter++)
         {
            const TargetCapacityPools* capPools = (*iter)->getTargetCapacityPools();

            capacityPoolsMap[(*iter)->getId()] = capPools->getPoolsAsLists();
         }
      } break;

      case CapacityPoolQuery_STORAGEBUDDIES:
      {
         const StoragePoolPtrVec storagePools = app->getStoragePoolStore()->getPoolsAsVec();

         for (auto iter = storagePools.begin(); iter != storagePools.end(); iter++)
         {
            const NodeCapacityPools* capPools = (*iter)->getBuddyCapacityPools();

            capacityPoolsMap[(*iter)->getId()] = capPools->getPoolsAsLists();
         }
      } break;

      default:
      {
         LogContext(logContext).logErr("Invalid pool type: " + StringTk::intToStr(poolType) );

         return false;
      } break;
   }

   ctx.sendResponse(GetNodeCapacityPoolsRespMsg(&capacityPoolsMap));

   return true;
}

