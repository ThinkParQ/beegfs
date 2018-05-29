#include "StoragePool.h"

void StoragePool::addTarget(uint16_t targetId, NumNodeID nodeId)
{
   std::lock_guard<Mutex> lock(mutex);

   addTargetUnlocked(targetId, nodeId);
}

/*
 * @param targetId
 * @param nodeId needed for targetCapacityPools
 */
void StoragePool::addTargetUnlocked(uint16_t targetId, NumNodeID nodeId)
{
   // note: if the target was already in the set, that's fine
   const auto insertRes = members.targets.insert(targetId);

   if (insertRes.second) // newly inserted
      targetsCapacityPools->addIfNotExists(targetId, nodeId, CapacityPool_LOW);
}

bool StoragePool::removeTarget(uint16_t targetId)
{
   std::lock_guard<Mutex> lock(mutex);

   return removeTargetUnlocked(targetId);
}

bool StoragePool::removeTargetUnlocked(uint16_t targetId)
{
   const size_t numErased = members.targets.erase(targetId);

   if (numErased > 0)
   {
      targetsCapacityPools->remove(targetId);
      return true;
   }
   else
   {
      return false;
   }
}

bool StoragePool::hasTarget(uint16_t targetId) const
{
   std::lock_guard<Mutex> lock(mutex);

   return (members.targets.find(targetId) != members.targets.end());
}

void StoragePool::addBuddyGroup(uint16_t buddyGroupId)
{
   std::lock_guard<Mutex> lock(mutex);

   addBuddyGroupUnlocked(buddyGroupId);
}


void StoragePool::addBuddyGroupUnlocked(uint16_t buddyGroupId)
{
   // note: if the target was already in the set, that's fine
   const auto insertRes = members.buddyGroups.insert(buddyGroupId);

   if (insertRes.second) // newly inserted
      buddyCapacityPools->addIfNotExists(buddyGroupId, CapacityPool_LOW);
}


bool StoragePool::removeBuddyGroup(uint16_t buddyGroupId)
{
   std::lock_guard<Mutex> lock(mutex);

   return removeBuddyGroupUnlocked(buddyGroupId);
}


bool StoragePool::removeBuddyGroupUnlocked(uint16_t buddyGroupId)
{
   const size_t numErased = members.buddyGroups.erase(buddyGroupId);

   if (numErased > 0)
   {
      buddyCapacityPools->remove(buddyGroupId);
      return true;
   }
   else
   {
      return false;
   }
}


bool StoragePool::hasBuddyGroup(uint16_t buddyGroupId) const
{
   std::lock_guard<Mutex> lock(mutex);

   return (members.buddyGroups.find(buddyGroupId) != members.buddyGroups.end());
}

void StoragePool::setDescription(const std::string& description)
{
   std::lock_guard<Mutex> lock(mutex);

   this->description = description;
}

StoragePoolId StoragePool::getId() const
{
   return id;
}

std::string StoragePool::getDescription() const
{
   std::lock_guard<Mutex> lock(mutex);

   return description;
}

UInt16Set StoragePool::getTargets() const
{
   std::lock_guard<Mutex> lock(mutex);

   return members.targets;
}

// remove all the targets in the pool and return a copy of them
UInt16Set StoragePool::getAndRemoveTargets()
{
   std::lock_guard<Mutex> lock(mutex);

   UInt16Set resultSet;

   resultSet.swap(members.targets);

   for (UInt16SetIter iter = resultSet.begin(); iter != resultSet.end(); iter++)
      targetsCapacityPools->remove(*iter);

   return resultSet;
}

UInt16Set StoragePool::getBuddyGroups() const
{
   std::lock_guard<Mutex> lock(mutex);

   return members.buddyGroups;
}

// remove all the buddy groups in the pool and return a copy of them
UInt16Set StoragePool::getAndRemoveBuddyGroups()
{
   std::lock_guard<Mutex> lock(mutex);

   UInt16Set resultSet;

   resultSet.swap(members.buddyGroups);

   for (UInt16SetIter iter = resultSet.begin(); iter != resultSet.end(); iter++)
      buddyCapacityPools->remove(*iter);

   return resultSet;
}
