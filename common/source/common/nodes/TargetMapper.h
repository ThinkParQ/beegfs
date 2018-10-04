#ifndef TARGETMAPPER_H_
#define TARGETMAPPER_H_

#include <common/nodes/StoragePoolStore.h>
#include <common/nodes/TargetCapacityPools.h>
#include <common/nodes/TargetStateStore.h>
#include <common/storage/quota/ExceededQuotaPerTarget.h>
#include <common/Common.h>


/**
 * Map targetIDs to nodeIDs.
 */
class TargetMapper
{
   public:
      TargetMapper();

      std::pair<FhgfsOpsErr, bool> mapTarget(uint16_t targetID, NumNodeID nodeID,
            StoragePoolId storagePoolId);
      bool unmapTarget(uint16_t targetID);
      bool unmapByNodeID(NumNodeID nodeID);

      void syncTargets(TargetMap newTargets);
      void getMappingAsLists(UInt16List& outTargetIDs, NumNodeIDList& outNodeIDs) const;
      void getTargetsByNode(NumNodeID nodeID, UInt16List& outTargetIDs) const;

      void attachStateStore(TargetStateStore* states);

      void attachStoragePoolStore(StoragePoolStore* storagePools);
      void attachExceededQuotaStores(ExceededQuotaPerTarget* exceededQuotaStores);

   protected:
      mutable RWLock rwlock;

      TargetMap targets; // keys: targetIDs, values: nodeNumIDs

      TargetStateStore* states; // optional for auto add/remove on map/unmap (may be NULL)
      StoragePoolStore* storagePools; // for auto add/remove on map/unmap (may be NULL)
      ExceededQuotaPerTarget* exceededQuotaStores; // for auto add/remove on map/unmap (may be NULL)


   public:
      // getters & setters

      /**
       * @return 0 if target not found
       */
      NumNodeID getNodeID(uint16_t targetID) const
      {
         RWLockGuard const lock(rwlock, SafeRWLock_READ);

         const auto iter = targets.find(targetID);
         return iter != targets.end()
            ? iter->second
            : NumNodeID{};
      }

      size_t getSize() const
      {
         RWLockGuard const lock(rwlock, SafeRWLock_READ);
         return targets.size();
      }

      bool targetExists(uint16_t targetID) const
      {
         RWLockGuard const lock(rwlock, SafeRWLock_READ);
         return targets.count(targetID) != 0;
      }

      TargetMap getMapping() const
      {
         RWLockGuard const lock(rwlock, SafeRWLock_READ);
         return targets;
      }
};

#endif /* TARGETMAPPER_H_ */
