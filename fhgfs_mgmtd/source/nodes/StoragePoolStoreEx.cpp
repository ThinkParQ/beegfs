#include "StoragePoolStoreEx.h"

#include <storage/StoragePoolEx.h>

StoragePoolPtr StoragePoolStoreEx::makePool(StoragePoolId id, const std::string& description) const
{
   // create a StoragePoolEx instead of a StoragePool as it is done in StoragePoolStore and
   // set appropriate dynamic capacity pool options
   const Config* cfg = Program::getApp()->getConfig();
   const DynamicPoolLimits poolLimitsStorageSpace(cfg->getTuneStorageSpaceLowLimit(),
         cfg->getTuneStorageSpaceEmergencyLimit(),
         cfg->getTuneStorageSpaceNormalSpreadThreshold(),
         cfg->getTuneStorageSpaceLowSpreadThreshold(),
         cfg->getTuneStorageSpaceLowDynamicLimit(),
         cfg->getTuneStorageSpaceEmergencyDynamicLimit() );
   const DynamicPoolLimits poolLimitsStorageInodes(cfg->getTuneStorageInodesLowLimit(),
         cfg->getTuneStorageInodesEmergencyLimit(),
         cfg->getTuneStorageInodesNormalSpreadThreshold(),
         cfg->getTuneStorageInodesLowSpreadThreshold(),
         cfg->getTuneStorageInodesLowDynamicLimit(),
         cfg->getTuneStorageInodesEmergencyDynamicLimit() );

   return std::make_shared<StoragePoolEx>(id, description, poolLimitsStorageSpace,
         poolLimitsStorageInodes, cfg->getTuneStorageDynamicPools());
}

StoragePoolPtr StoragePoolStoreEx::makePool() const
{
   // create a StoragePoolEx instead of a StoragePool as it is done in StoragePoolStore and
   // set appropriate dynamic capacity pool options
   const Config* cfg = Program::getApp()->getConfig();
   const DynamicPoolLimits poolLimitsStorageSpace(cfg->getTuneStorageSpaceLowLimit(),
         cfg->getTuneStorageSpaceEmergencyLimit(),
         cfg->getTuneStorageSpaceNormalSpreadThreshold(),
         cfg->getTuneStorageSpaceLowSpreadThreshold(),
         cfg->getTuneStorageSpaceLowDynamicLimit(),
         cfg->getTuneStorageSpaceEmergencyDynamicLimit() );
   const DynamicPoolLimits poolLimitsStorageInodes(cfg->getTuneStorageInodesLowLimit(),
         cfg->getTuneStorageInodesEmergencyLimit(),
         cfg->getTuneStorageInodesNormalSpreadThreshold(),
         cfg->getTuneStorageInodesLowSpreadThreshold(),
         cfg->getTuneStorageInodesLowDynamicLimit(),
         cfg->getTuneStorageInodesEmergencyDynamicLimit() );

   return std::make_shared<StoragePoolEx>(INVALID_POOL_ID, "", poolLimitsStorageSpace,
         poolLimitsStorageInodes, cfg->getTuneStorageDynamicPools());
}

StoragePoolExPtr StoragePoolStoreEx::getPool(StoragePoolId id) const
{
   StoragePoolPtr pool = StoragePoolStore::getPool(id);
   return std::static_pointer_cast<StoragePoolEx>(pool);
}

StoragePoolExPtr StoragePoolStoreEx::getPool(uint16_t targetId) const
{
   StoragePoolPtr pool = StoragePoolStore::getPool(targetId);
   return std::static_pointer_cast<StoragePoolEx>(pool);
}

StoragePoolExPtrVec StoragePoolStoreEx::getPoolsAsVec() const
{
   StoragePoolExPtrVec outVec;

   RWLockGuard lock(rwlock, SafeRWLock_READ);

   for (auto it = storagePools.begin(); it != storagePools.end(); it++)
      outVec.push_back(std::static_pointer_cast<StoragePoolEx>(it->second));

   return outVec;
}

FhgfsOpsErr StoragePoolStoreEx::addTarget(uint16_t targetId, NumNodeID nodeId, StoragePoolId poolId,
   bool ignoreExisting)
{
   FhgfsOpsErr addRes = StoragePoolStore::addTarget(targetId, nodeId, poolId, ignoreExisting);

   // make sure pools get synced
   if (addRes == FhgfsOpsErr_SUCCESS)
   {
      if (heartbeatManager) // heartbeat manager might not have been initialized, yet!
         heartbeatManager->notifyAsyncRefreshStoragePools();
   }

   return addRes;
}

StoragePoolId StoragePoolStoreEx::removeTarget(uint16_t targetId)
{
   StoragePoolId poolId = StoragePoolStore::removeTarget(targetId);
   // make sure pools get synced
   if (heartbeatManager) // heartbeat manager might not have been initialized, yet!
      heartbeatManager->notifyAsyncRefreshStoragePools();
   return poolId;
}

FhgfsOpsErr StoragePoolStoreEx::addBuddyGroup(uint16_t buddyGroupId, StoragePoolId poolId,
      bool ignoreExisting)
{
   FhgfsOpsErr addRes = StoragePoolStore::addBuddyGroup(buddyGroupId, poolId, ignoreExisting);

   // make sure pools get synced
   if (addRes == FhgfsOpsErr_SUCCESS)
   {
      if (heartbeatManager) // heartbeat manager might not have been initialized, yet!
         heartbeatManager->notifyAsyncRefreshStoragePools();
   }

   return addRes;
}


StoragePoolId StoragePoolStoreEx::removeBuddyGroup(uint16_t buddyGroupId)
{
   StoragePoolId poolId = StoragePoolStore::removeBuddyGroup(buddyGroupId);
   // make sure pools get synced
   if (heartbeatManager) // heartbeat manager might not have been initialized, yet!
      heartbeatManager->notifyAsyncRefreshStoragePools();

   return poolId;
}

FhgfsOpsErr StoragePoolStoreEx::removePool(StoragePoolId poolId)
{
   if (poolId == StoragePoolStore::DEFAULT_POOL_ID)
      return FhgfsOpsErr_INVAL; // never remove the default pool

   // first reference the pool as well as the default pool and move all targets from the pool to
   // default
   StoragePoolExPtr rmPool = getPool(poolId);
   if (!rmPool)
      return FhgfsOpsErr_UNKNOWNPOOL; // pool to remove does not exist

   StoragePoolExPtr defaultPool = getPool(StoragePoolStore::DEFAULT_POOL_ID);

   UInt16Set targets = rmPool->getAndRemoveTargets();
   for (auto iter = targets.begin(); iter != targets.end(); iter++)
   {
      // get node mapping for target; of course this is not optimal, but considering the fact
      // that this doesn't happen frequently it's ok
      const NumNodeID nodeId = targetMapper->getNodeID(*iter);
      defaultPool->addTarget(*iter, nodeId);
   }

   UInt16Set buddyGroups = rmPool->getAndRemoveBuddyGroups();
   for (auto iter = buddyGroups.begin(); iter != buddyGroups.end(); iter++)
   {
      defaultPool->addBuddyGroup(*iter);
   }

   // remove quota information from disk
   rmPool->clearQuota();

   // delete the pool
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);

   storagePools.erase(poolId);

   mappingsDirty = true;

   return FhgfsOpsErr_SUCCESS;
}
