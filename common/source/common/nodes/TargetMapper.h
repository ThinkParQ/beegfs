#ifndef TARGETMAPPER_H_
#define TARGETMAPPER_H_

#include <common/nodes/StoragePoolStore.h>
#include <common/nodes/TargetCapacityPools.h>
#include <common/nodes/TargetStateStore.h>
#include <common/storage/quota/ExceededQuotaPerTarget.h>
#include <common/threading/SafeRWLock.h>
#include <common/threading/SafeMutexLock.h>
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

      void syncTargetsFromLists(UInt16List& targetIDs, NumNodeIDList& nodeIDs);
      void getMappingAsLists(UInt16List& outTargetIDs, NumNodeIDList& outNodeIDs) const;
      void getMapping(TargetMap& outTargetMap) const;
      void getTargetsByNode(NumNodeID nodeID, UInt16List& outTargetIDs) const;

      void attachStateStore(TargetStateStore* states);

      void attachStoragePoolStore(StoragePoolStore* storagePools);
      void attachExceededQuotaStores(ExceededQuotaPerTarget* exceededQuotaStores);

      bool loadFromFile();
      bool saveToFile();


   private:
      mutable RWLock rwlock;
      TargetMap targets; // keys: targetIDs, values: nodeNumIDs

      TargetStateStore* states; // optional for auto add/remove on map/unmap (may be NULL)
      StoragePoolStore* storagePools; // for auto add/remove on map/unmap (may be NULL)
      ExceededQuotaPerTarget* exceededQuotaStores; // for auto add/remove on map/unmap (may be NULL)

      bool mappingsDirty; // true if saved mappings file needs to be updated
      std::string storePath; // set to enable load/save methods (setting is not thread-safe)

      void exportToStringMap(StringMap& outExportMap) const;
      void importFromStringMap(StringMap& importMap);


   public:
      // getters & setters

      /**
       * @return 0 if target not found
       */
      NumNodeID getNodeID(uint16_t targetID) const
      {
         NumNodeID nodeID;

         RWLockGuard safeLock(rwlock, SafeRWLock_READ); // L O C K

         TargetMapCIter iter = targets.find(targetID);
         if(iter != targets.end() )
         {
            nodeID = iter->second;
         }

         return nodeID;
      }

      size_t getSize() const
      {
         RWLockGuard safeLock(rwlock, SafeRWLock_READ); // L O C K

         size_t targetsSize = targets.size();

         return targetsSize;
      }

      void setStorePath(std::string storePath)
      {
         this->storePath = storePath;
      }

      bool isMapperDirty() const
      {
         RWLockGuard safeLock(rwlock, SafeRWLock_READ); // L O C K

         bool retVal = mappingsDirty;

         return retVal;
      }

      bool targetExists(uint16_t targetID) const
      {
         bool retVal = false;

         RWLockGuard safeLock(rwlock, SafeRWLock_READ); // L O C K

         for (TargetMapCIter iter = targets.begin(); iter != targets.end(); iter++)
         {
            if (iter->first == targetID)
            {
               retVal = true;
               break;
            }
         }

         return retVal;
      }
};

#endif /* TARGETMAPPER_H_ */
