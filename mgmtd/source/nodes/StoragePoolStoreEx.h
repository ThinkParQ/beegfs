#ifndef MGMTD_STORAGEPOOLSTOREEX_H_
#define MGMTD_STORAGEPOOLSTOREEX_H_

#include <common/nodes/StoragePoolStore.h>
#include <components/HeartbeatManager.h>

class StoragePoolEx;
typedef std::shared_ptr<StoragePoolEx> StoragePoolExPtr;
typedef std::vector<StoragePoolExPtr> StoragePoolExPtrVec;

/*
 * special implementation for mgmtd, which creates StoragePoolEx objects with an attached
 * QuotaManager
 */
class StoragePoolStoreEx: public StoragePoolStore
{
   public:
      StoragePoolStoreEx(MirrorBuddyGroupMapper* buddyGroupMapper, TargetMapper* targetMapper):
         StoragePoolStore(buddyGroupMapper, targetMapper, true), heartbeatManager(nullptr)
      {
         createPool(DEFAULT_POOL_ID, DEFAULT_POOL_NAME);
      }

      void attachHeartbeatManager(HeartbeatManager* heartbeatManager)
      {
         this->heartbeatManager = heartbeatManager;
      }

      StoragePoolExPtr getPool(StoragePoolId id) const;
      StoragePoolExPtr getPool(uint16_t targetId) const;

      StoragePoolExPtrVec getPoolsAsVec() const;

      virtual FhgfsOpsErr addTarget(uint16_t targetId, NumNodeID nodeId,
         StoragePoolId poolId = DEFAULT_POOL_ID, bool ignoreExisting = false) override;
      virtual StoragePoolId removeTarget(uint16_t targetId) override;
      virtual FhgfsOpsErr addBuddyGroup(uint16_t buddyGroupId, StoragePoolId poolId,
         bool ignoreExisting = false) override;
      virtual StoragePoolId removeBuddyGroup(uint16_t buddyGroupId) override;

      // note: only implemented for StoragePoolStoreEx, because only needed in mgmtd
      FhgfsOpsErr removePool(StoragePoolId poolId);

      bool loadFromFile();
      bool saveToFile();

      void setStorePath(const std::string& storePath);

   private:
      HeartbeatManager* heartbeatManager; // used to send notifications to servers

      StoragePoolPtr makePool(StoragePoolId id, const std::string& description) const override;
      StoragePoolPtr makePool() const override;

      std::string storePath; // set to enable load/save methods (setting is not thread-safe)
};


#endif /* MGMTD_STORAGEPOOLSTOREEX_H_ */
