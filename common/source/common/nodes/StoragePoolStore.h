#ifndef COMMON_STORAGEPOOLSTORE_H_
#define COMMON_STORAGEPOOLSTORE_H_

#include <common/storage/StoragePool.h>
#include <common/threading/RWLockGuard.h>

class MirrorBuddyGroupMapper;
class TargetMapper;

/*
 * holds the storage pools and the assigned targets
 */
class StoragePoolStore
{
   public:
      static const StoragePoolId DEFAULT_POOL_ID;
      static const StoragePoolId INVALID_POOL_ID;
      static const std::string DEFAULT_POOL_NAME;

      StoragePoolStore(MirrorBuddyGroupMapper* buddyGroupMapper,
                       TargetMapper* targetMapper, bool skipDefPoolCreation = false);
      virtual ~StoragePoolStore() { };

      std::pair<FhgfsOpsErr, StoragePoolId> createPool(StoragePoolId poolId,
         const std::string& poolDescription, const UInt16Set* targets = NULL,
         const UInt16Set* buddyGroups = NULL);

      FhgfsOpsErr moveTarget(StoragePoolId fromPoolId, StoragePoolId toPoolId, uint16_t targetId);
      FhgfsOpsErr moveBuddyGroup(StoragePoolId fromPoolId, StoragePoolId toPoolId,
         uint16_t buddyGroupId);

      FhgfsOpsErr setPoolDescription(StoragePoolId poolId, const std::string& newDescription);

      virtual FhgfsOpsErr addTarget(uint16_t targetId, NumNodeID nodeId,
         StoragePoolId poolId = DEFAULT_POOL_ID, bool ignoreExisting = false);
      virtual StoragePoolId removeTarget(uint16_t targetId);
      virtual FhgfsOpsErr addBuddyGroup(uint16_t buddyGroupId, StoragePoolId poolId,
         bool ignoreExisting = false);
      virtual StoragePoolId removeBuddyGroup(uint16_t buddyGroupId);

      void syncFromVector(const StoragePoolPtrVec& storagePoolVec);

      StoragePoolPtr getPool(StoragePoolId id) const;
      StoragePoolPtr getPool(uint16_t targetId) const;
      StoragePoolPtr getPoolUnlocked(StoragePoolId id) const;
      StoragePoolPtrVec getPoolsAsVec() const;
      bool poolExists(StoragePoolId id) const;

      size_t getSize() const;

   protected:
      static const StoragePoolId MAX_POOL_ID;

      mutable RWLock rwlock; // protecting the storage pool map
      StoragePoolPtrMap storagePools;

      MirrorBuddyGroupMapper* buddyGroupMapper;
      TargetMapper* targetMapper;

      StoragePoolId findFreeID() const;
      bool moveTargetUnlocked(StoragePoolPtr fromPool, StoragePoolPtr toPool, uint16_t targetId);
      bool moveBuddyGroupUnlocked(StoragePoolPtr fromPool, StoragePoolPtr toPool,
         uint16_t buddyGroupId);

      virtual StoragePoolPtr makePool(StoragePoolId id, const std::string& description) const
      {
         return std::make_shared<StoragePool>(id, description);
      }

      virtual StoragePoolPtr makePool() const
      {
         return std::make_shared<StoragePool>();
      }

      /*
       * note: no locking in here; so make sure, caller holds lock
       */
      void serialize(Serializer& ser) const
      {
         // simply serialize the map's values; keys can be generated again from the target pool IDs
         ser % storagePools.size();

         for (StoragePoolPtrMapCIter it = storagePools.begin(); it != storagePools.end(); it++)
            ser % *(it->second);
      }

      /*
       * note: no locking in here; so make sure, caller holds lock
       */
      void deserialize(Deserializer& des)
      {
         size_t elemCount;

         des % elemCount;

         storagePools.clear();

         for (size_t i = 0; i < elemCount; i++)
         {
            StoragePoolPtr elemPtr = makePool();
            // we call initFromDesBuf() here instead of the regular deserializer because this store
            // might hold StoragePool or StoragePoolEx elements and initFromDesBuf() is a virtual
            // funtion, wrapping around the "right" deserializer
            bool desResult = elemPtr->initFromDesBuf(des);
            if (!desResult)
               break;

            storagePools[elemPtr->getId()] = elemPtr;
         }
      }
};

#endif /* COMMON_STORAGEPOOLSTORE_H_ */
