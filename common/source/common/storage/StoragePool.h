#ifndef COMMON_STORAGEPOOL_H
#define COMMON_STORAGEPOOL_H

#include <common/nodes/DynamicPoolLimits.h>
#include <common/nodes/NodeCapacityPools.h>
#include <common/nodes/TargetCapacityPools.h>
#include <common/storage/StoragePoolId.h>
#include <common/threading/Mutex.h>

#include <mutex>
#include <unordered_map>

/*
 * represents a storage target pool
 */
class StoragePool
{
   friend class StoragePoolStore; // for access to the internal mutex and unlocked functions

   public:
      StoragePool(StoragePoolId id, const std::string& description,
         const DynamicPoolLimits& capPoolLimitsStorageSpace,
         const DynamicPoolLimits& capPoolLimitsStorageInodes, bool useDynamicCapPools):
         id(id), description(description)
      {
         targetsCapacityPools = std::make_shared<TargetCapacityPools>(
               useDynamicCapPools, capPoolLimitsStorageSpace, capPoolLimitsStorageInodes);
         buddyCapacityPools = std::make_shared<NodeCapacityPools>(
               false, DynamicPoolLimits(0, 0, 0, 0, 0, 0), DynamicPoolLimits(0, 0, 0, 0, 0, 0));
      }

      StoragePool(StoragePoolId id = StoragePoolId(0), const std::string& description = "") :
         id(id), description(description)
      {
         targetsCapacityPools = std::make_shared<TargetCapacityPools>(
               false, DynamicPoolLimits(0, 0, 0, 0, 0, 0), DynamicPoolLimits(0, 0, 0, 0, 0, 0));
         buddyCapacityPools = std::make_shared<NodeCapacityPools>(
               false, DynamicPoolLimits(0, 0, 0, 0, 0, 0), DynamicPoolLimits(0, 0, 0, 0, 0, 0));
      }

      StoragePool(const StoragePool& rhs) = delete;

      virtual ~StoragePool() = default;

      StoragePoolId getId() const;
      std::string getDescription() const;
      void setDescription(const std::string& description);

      UInt16Set getTargets() const;
      UInt16Set getAndRemoveTargets();
      void addTarget(uint16_t targetId, NumNodeID nodeId);
      bool removeTarget(uint16_t targetId);
      bool hasTarget(uint16_t targetId) const;

      UInt16Set getBuddyGroups() const;
      UInt16Set getAndRemoveBuddyGroups();
      void addBuddyGroup(uint16_t buddyGroupId);
      bool removeBuddyGroup(uint16_t buddyGroupId);
      bool hasBuddyGroup(uint16_t buddyGroupId) const;

      TargetCapacityPools* getTargetCapacityPools() const
      {
         return targetsCapacityPools.get();
      }

      NodeCapacityPools* getBuddyCapacityPools() const
      {
         return buddyCapacityPools.get();
      }

      // note: capacity pool info is also serialized
      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
          ctx
             % obj->id
             % obj->description
             % obj->members.targets
             % obj->members.buddyGroups
             % *(obj->targetsCapacityPools)
             % *(obj->buddyCapacityPools);
      }

      friend Serializer& operator%(Serializer& ser, const std::shared_ptr<StoragePool>& obj)
      {
         ser % *obj;

         return ser;
      }

      friend Deserializer& operator%(Deserializer& des, std::shared_ptr<StoragePool>& obj)
      {
         obj.reset(new StoragePool);

         des % *obj;

         return des;
      }

      /*
       * wrapper around deserializer to perform additional tasks after deserialization in inherited
       * classes
       */
      virtual bool initFromDesBuf(Deserializer& des)
      {
         StoragePool::serialize(this, des);

         return des.good();
      }

      // comparisions are done using the numeric ID
      bool operator==(const StoragePool& rhs) const
      {
         return (id == rhs.id);
      }

      bool operator!=(const StoragePool& rhs) const
      {
         return (id != rhs.id);
      }

      bool operator<(const StoragePool& rhs) const
      {
         return (id < rhs.id);
      }

      bool operator>(const StoragePool& rhs) const
      {
         return (id > rhs.id);
      }

   protected:
      StoragePoolId id; // a numeric ID for the pool
      std::string description; // user defined description

      mutable Mutex mutex; // protecting targets and buddyGroups
      struct {
         UInt16Set targets;     // targets that belong to the pool
         UInt16Set buddyGroups; // mirror buddygroups that belong to the pool
      } members;

      std::shared_ptr<TargetCapacityPools> targetsCapacityPools;
      std::shared_ptr<NodeCapacityPools> buddyCapacityPools;

      void addTargetUnlocked(uint16_t targetId, NumNodeID nodeId);
      bool removeTargetUnlocked(uint16_t targetId);
      void addBuddyGroupUnlocked(uint16_t buddyGroupId);
      bool removeBuddyGroupUnlocked(uint16_t buddyGroupId);
};

typedef std::shared_ptr<StoragePool> StoragePoolPtr;

typedef std::vector<StoragePool> StoragePoolVec;
typedef StoragePoolVec::iterator StoragePoolVecIter;
typedef StoragePoolVec::const_iterator StoragePoolVecCIter;

typedef std::vector<StoragePoolPtr> StoragePoolPtrVec;
typedef StoragePoolPtrVec::iterator StoragePoolPtrVecIter;
typedef StoragePoolPtrVec::const_iterator StoragePoolPtrVecCIter;

typedef std::map<StoragePoolId, StoragePoolPtr> StoragePoolPtrMap;
typedef StoragePoolPtrMap::iterator StoragePoolPtrMapIter;
typedef StoragePoolPtrMap::const_iterator StoragePoolPtrMapCIter;

#endif /*COMMON_STORAGEPOOL_H*/
