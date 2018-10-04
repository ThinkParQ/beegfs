#include <common/net/message/nodes/GetNodeCapacityPoolsRespMsg.h>
#include <common/nodes/TargetCapacityPools.h>
#include <program/Program.h>
#include <storage/StoragePoolEx.h>
#include "GetNodeCapacityPoolsMsgEx.h"

bool GetNodeCapacityPoolsMsgEx::processIncoming(ResponseContext& ctx)
{
   CapacityPoolQueryType poolType = getCapacityPoolQueryType();

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

      case CapacityPoolQuery_METABUDDIES:
      {
         const NodeCapacityPools* capPools = app->getMetaBuddyCapacityPools();
         capacityPoolsMap[StoragePoolId(StoragePoolStore::INVALID_POOL_ID)] =
               capPools->getPoolsAsLists();
      } break;

      case CapacityPoolQuery_STORAGE:
      {
         const StoragePoolExPtrVec storagePools = app->getStoragePoolStore()->getPoolsAsVec();

         for (auto iter = storagePools.begin(); iter != storagePools.end(); iter++)
         {
            const TargetCapacityPools* capPools = (*iter)->getTargetCapacityPools();
            capacityPoolsMap[(*iter)->getId()] = capPools->getPoolsAsLists();
         }
      } break;

      case CapacityPoolQuery_STORAGEBUDDIES:
      {
         const StoragePoolExPtrVec storagePools = app->getStoragePoolStore()->getPoolsAsVec();

         for (auto iter = storagePools.begin(); iter != storagePools.end(); iter++)
         {
            const NodeCapacityPools* capPools = (*iter)->getBuddyCapacityPools();
            capacityPoolsMap[(*iter)->getId()] = capPools->getPoolsAsLists();
         }
      } break;

      default:
      {
         LOG(CAPACITY, ERR, "Invalid pool type.", getCapacityPoolQueryType());
         return false;
      } break;
   }

   ctx.sendResponse(GetNodeCapacityPoolsRespMsg(&capacityPoolsMap));

   return true;
}

