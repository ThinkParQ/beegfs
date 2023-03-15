#include <common/toolkit/TempFileTk.h>
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

/**
 * @return true if successfully loaded, false if not
 *
 * Note: setStorePath must be called before using this.
 */
bool StoragePoolStoreEx::loadFromFile()
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);

   if (storePath.empty())
      return false;

   // create/trunc file
   const int openFlags = O_RDONLY;

   int fd = open(storePath.c_str(), openFlags, 0);
   if (fd == -1)
   { // error
      LOG(STORAGEPOOLS, ERR, "Could not open storage pool mappings file.", storePath, sysErr);

      return false;
   }

   // file open -> read contents
   bool retVal;

   try
   {
      // determine buffer length
      struct stat statBuf;
      int statRes = fstat(fd, &statBuf);

      if (statRes != 0)
      { // stat failed
         LOG(STORAGEPOOLS, ERR, "Could not stat storage pool mappings file.", storePath, sysErr);

         retVal = false;
         goto cleanup_and_exit;
      }

      // allocate memory for contents
      boost::scoped_array<char> buf(new char[statBuf.st_size]);

      // read from file
      const ssize_t readRes = read(fd, buf.get(), statBuf.st_size);

      if(readRes == -1)
      { // reading failed
         LOG(STORAGEPOOLS, ERR, "Could not read from storage pool mappings file.",
             storePath, sysErr);

         retVal = false;
         goto cleanup_and_exit;
      }
      else
      {  // do the actual deserialization
         Deserializer des(buf.get(), readRes);
         deserialize(des);
         retVal = des.good();

         if (!retVal)
            LOG(STORAGEPOOLS, ERR, "Could not deserialize storage pool mappings file.", storePath);
      }
   }
   catch (const std::bad_alloc& e)
   {
      LOG(STORAGEPOOLS, ERR,
          "Could not allocate memory for contents of storage pool mappings file.", storePath);
      retVal = false;
   }

cleanup_and_exit:
   close (fd);

   return retVal;
}


/**
 * @return true if successfully stored, false if not
 *
 * Note: setStorePath must be called before using this.
 */
bool StoragePoolStoreEx::saveToFile()
{
   LogContext log("StoragePoolStoreEx (save)");

   RWLockGuard lock(rwlock, SafeRWLock_READ);

   if (storePath.empty())
      return false;

   Serializer lengthSerializer;
   serialize(lengthSerializer);
   const unsigned bufLen = lengthSerializer.size();

   // do the actual serialization
   boost::scoped_array<char> buf(new char[bufLen]);

   Serializer ser(buf.get(), bufLen);
   serialize(ser);
   if (!ser.good())
   {
      LOG(STORAGEPOOLS, ERR, "Could not serialize storage pool mappings file.", storePath);
      return false;
   }

   if (TempFileTk::storeTmpAndMove(storePath.c_str(), buf.get(), bufLen) == FhgfsOpsErr_SUCCESS)
   {
      LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Nodes file stored: " + storePath);
      return true;
   }
   else
      return false;
}


void StoragePoolStoreEx::setStorePath(const std::string& storePath)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);

   this->storePath = storePath;
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

   return FhgfsOpsErr_SUCCESS;
}
