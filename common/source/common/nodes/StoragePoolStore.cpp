#include "StoragePoolStore.h"

#include <common/nodes/MirrorBuddyGroupMapper.h>

const StoragePoolId StoragePoolStore::DEFAULT_POOL_ID = StoragePoolId(1);
const StoragePoolId StoragePoolStore::INVALID_POOL_ID = StoragePoolId(0);
const std::string StoragePoolStore::DEFAULT_POOL_NAME = "Default";
const StoragePoolId StoragePoolStore::MAX_POOL_ID = std::numeric_limits<StoragePoolId>::max();

StoragePoolStore::StoragePoolStore(MirrorBuddyGroupMapper* buddyGroupMapper,
                                   TargetMapper* targetMapper, bool skipDefPoolCreation):
   buddyGroupMapper(buddyGroupMapper), targetMapper(targetMapper)
{
   if (!skipDefPoolCreation)
   {
      // create the default pool, which must exist at all times
      createPool(DEFAULT_POOL_ID, DEFAULT_POOL_NAME);
   }
}

/*
 * @param poolID the ID of the newly created pool; can be INVALID_POOL_ID, in which case a free ID
 *        will be chosen
 * @param poolDescription a description for the pool
 * @param targets a set of targets to add to the new pool; can be (and defaults to) NULL
 *        (which results in an empty pool)
 * @param buddyGroups a set of buddyGroups to add to the new pool; can be (and defaults to) NULL
 * @return a pair with the return code as first element (FhgfsOpsErr_NOSPACE if no free ID could
 *         be found; FhgfsOpsErr_EXISTS if ID was given and a pool with that ID already exists;
 *         FhgfsOpsErr_INVAL if at least one target could not be moved, because it is not in the
 *         default pool; FhgfsOpsErr_SUCCESS otherwise) and the ID of the inserted pool as second
 *         element
 */
std::pair<FhgfsOpsErr, StoragePoolId> StoragePoolStore::createPool(StoragePoolId poolId,
   const std::string& poolDescription, const UInt16Set* targets, const UInt16Set* buddyGroups)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);

   if (poolId == INVALID_POOL_ID)
   {
      poolId = findFreeID();

      LOG(STORAGEPOOLS, DEBUG, "Generated new target pool ID.", poolId);

      // pool ID still zero? -> could not find any free ID
      if (poolId == INVALID_POOL_ID)
      {
         LOG(STORAGEPOOLS, WARNING, "Could not generate storage pool ID. "
                                    "Probably all valid IDs are in use.");

         return std::make_pair(FhgfsOpsErr_NOSPACE, INVALID_POOL_ID);
      }
   }

   StoragePoolPtr newStoragePool = makePool(poolId, poolDescription);

   std::pair<StoragePoolPtrMapIter, bool> insertRes =
      storagePools.insert(std::pair<StoragePoolId, StoragePoolPtr>(poolId, newStoragePool));

   if (insertRes.second) // newly inserted element
   {
      unsigned failedMoves = 0;

      // handle buddy groups first
      if (buddyGroups)
      {
         // Check if buddy groups are in the default pool and only allow to put them into new pool
         // if they are (and delete them from default pool). This way we don't allow a buddy group
         // to be in more than one pool

         StoragePoolPtr defaultPool = storagePools[DEFAULT_POOL_ID];

         for (auto groupIter = buddyGroups->begin(); groupIter != buddyGroups->end(); groupIter++)
         {
            if (defaultPool->hasBuddyGroup(*groupIter))
            {
               moveBuddyGroupUnlocked(defaultPool, insertRes.first->second, *groupIter);
            }
            else
            {
               LOG(STORAGEPOOLS, WARNING, "Couldn't move buddy group to newly created storage pool,"
                            " because buddy group ID is not a member of the default pool.",
                            ("newPoolId", poolId), ("buddyGroupId", *groupIter));

               ++failedMoves;
            }
         }
      }

      if (targets)
      {
         // Check if targets are in the default pool and only allow to put them into new pool if
         // they are (and delete them from default pool). This way we don't allow a target to be in
         // more than one pool

         StoragePoolPtr defaultPool = storagePools[DEFAULT_POOL_ID];

         for (auto targetIter = targets->begin(); targetIter != targets->end(); targetIter++)
         {
            if (defaultPool->hasTarget(*targetIter))
            {
               moveTargetUnlocked(defaultPool, insertRes.first->second, *targetIter);
            }
            else
            {
               LOG(STORAGEPOOLS, WARNING, "Couldn't move target to newly created storage pool, "
                                          "because target ID is not a member of the default pool.",
                                          ("newPoolId", poolId), ("targetId", *targetIter));

               ++failedMoves;
            }
         }
      }

      if (failedMoves > 0)
         return std::make_pair(FhgfsOpsErr_INVAL, poolId);
      else
         return std::make_pair(FhgfsOpsErr_SUCCESS, poolId);
   }
   else
   {
      LOG(STORAGEPOOLS, WARNING,
         "Could not insert new storage pool, because a pool with the given ID already exists.",
         poolId);

      return std::make_pair(FhgfsOpsErr_EXISTS, INVALID_POOL_ID);
   }
}

/*
 * moves a target from one pool to another
 *
 * @param fromPoolId
 * @param toPoolId
 * @param targetId
 *
 * @return FhgfsOpsErr_UNKNOWNPOOL if sourcePoolId or destinationPoolId unknown,
 * FhgfsOpsErr_UNKNOWNTARGET if target is not in source pool, FhgfsOpsErr_SUCCESS on successful
 * move
 */
FhgfsOpsErr StoragePoolStore::moveTarget(StoragePoolId fromPoolId, StoragePoolId toPoolId,
   uint16_t targetId)
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);

   StoragePoolPtr fromPool = getPoolUnlocked(fromPoolId);
   StoragePoolPtr toPool = getPoolUnlocked(toPoolId);

   if (!fromPool || !toPool)
      return FhgfsOpsErr_UNKNOWNPOOL;

   bool mvRes = moveTargetUnlocked(fromPool, toPool, targetId);

   if (!mvRes)
      return FhgfsOpsErr_UNKNOWNTARGET;

   return FhgfsOpsErr_SUCCESS;
}

bool StoragePoolStore::moveTargetUnlocked(StoragePoolPtr fromPool, StoragePoolPtr toPool,
   uint16_t targetId)
{
   const bool rmRes = fromPool->removeTarget(targetId);

   if (!rmRes)
   {
      LOG(STORAGEPOOLS, WARNING, "Could not move target from one storage pool to another. "
         "Target does not exist in old pool.",
         ("oldPoolID", fromPool->getId()), ("newPoolID", toPool->getId()), targetId);

      return false;
   }

   // get node mapping for target; of course this is not optimal, but considering the fact that
   // this doesn't happen frequently it's ok
   const NumNodeID nodeId = targetMapper->getNodeID(targetId);
   toPool->addTarget(targetId, nodeId);

   return true;
}

FhgfsOpsErr StoragePoolStore::moveBuddyGroup(StoragePoolId fromPoolId, StoragePoolId toPoolId,
   uint16_t buddyGroupId)
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);

   StoragePoolPtr fromPool = getPoolUnlocked(fromPoolId);
   StoragePoolPtr toPool = getPoolUnlocked(toPoolId);

   if (!fromPool || !toPool)
      return FhgfsOpsErr_UNKNOWNPOOL;

   const bool mvRes = moveBuddyGroupUnlocked(fromPool, toPool, buddyGroupId);

   if (!mvRes)
      return FhgfsOpsErr_UNKNOWNTARGET;

   return FhgfsOpsErr_SUCCESS;
}

/*
 * note: when moving buddy groups, we also move around the targets of this buddy group if a
 * buddyGroupMapper is set
 */
bool StoragePoolStore::moveBuddyGroupUnlocked(StoragePoolPtr fromPool, StoragePoolPtr toPool,
   uint16_t buddyGroupId)
{
   // both pools are locked from the outside here, because we need to prevent races between the
   // rm/add of buddy groups and targets
   std::lock_guard<Mutex> fromPoolLock(fromPool->mutex);
   std::lock_guard<Mutex> toPoolLock(toPool->mutex);

   const bool rmRes = fromPool->removeBuddyGroupUnlocked(buddyGroupId);

   if (!rmRes)
   {
      LOG(STORAGEPOOLS, WARNING, "Could not move buddy group from one storage pool to another, "
         "because buddy group doesn't exist in old pool.",
         ("oldPoolID", fromPool->getId()), ("newPoolID", toPool->getId()), buddyGroupId);

      return false;
   }

   toPool->addBuddyGroupUnlocked(buddyGroupId);

   if (buddyGroupMapper)
   {
      const MirrorBuddyGroup mbg = buddyGroupMapper->getMirrorBuddyGroup(buddyGroupId);
      const bool rmPrimaryRes = fromPool->removeTargetUnlocked(mbg.firstTargetID);

      if (rmPrimaryRes)
      {
         // get node mapping for target; of course this is not optimal, but considering the fact
         // that this doesn't happen frequently it's ok
         const NumNodeID nodeId = targetMapper->getNodeID(mbg.firstTargetID);
         toPool->addTargetUnlocked(mbg.firstTargetID, nodeId);
      }

      const bool rmSecondaryRes = fromPool->removeTargetUnlocked(mbg.secondTargetID);

      if (rmSecondaryRes)
      {
         // get node mapping for target; of course this is not optimal, but considering the fact
         // that this doesn't happen frequently it's ok
         const NumNodeID nodeId = targetMapper->getNodeID(mbg.secondTargetID);
         toPool->addTargetUnlocked(mbg.secondTargetID, nodeId);
      }

      if (!rmPrimaryRes || !rmSecondaryRes)
      {
         LOG(STORAGEPOOLS, WARNING, "Could not move targets of buddy group from one storage pool to another, "
            "because at least one target doesn't exist in old pool.",
            ("oldPoolID", fromPool->getId()), ("newPoolID", toPool->getId()), buddyGroupId,
            ("primaryTargetId", mbg.firstTargetID), ("secondaryTargetId", mbg.secondTargetID));

         return false;
      }
   }

   return true;
}

FhgfsOpsErr StoragePoolStore::setPoolDescription(StoragePoolId poolId,
   const std::string& newDescription)
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);

   StoragePoolPtr pool = getPoolUnlocked(poolId);

   if (!pool)
      return FhgfsOpsErr_UNKNOWNPOOL;

   pool->setDescription(newDescription);

   return FhgfsOpsErr_SUCCESS;
}

/*
 * add a target to the pool store
 *
 * @param targetId
 * @param poolId add to this pool id if target is inserted for the first time;
 * @param ignoreExisting if set to true it will not be considered an error if the target is already
 *        member of a pool; in this case the request is silently ignored
 * @return FhgfsOpsErr_SUCCESS if operation was succesful
 *         FhgfsOpsErr_EXISTS if target is already member of a pool and !ignoreExisting
 *         FhgfsOpsErr_UNKNOWNPOOL if poolId doesn't belong to a valid pool
 */
FhgfsOpsErr StoragePoolStore::addTarget(uint16_t targetId, NumNodeID nodeId, StoragePoolId poolId,
      bool ignoreExisting)
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);

   // first check if this target is in any of the pools already
   for (auto poolsIter = storagePools.begin(); poolsIter != storagePools.end(); poolsIter++)
   {
      if (poolsIter->second->hasTarget(targetId))
      {
         if (!ignoreExisting)
         {
            LOG(STORAGEPOOLS, ERR, "Requested to add target to storage pool store, "
                     "but target is already member of a storage pool.",
                     targetId, ("storagePoolId", poolsIter->second->getId()));

            return FhgfsOpsErr_EXISTS;
         }
         return FhgfsOpsErr_SUCCESS;
      }
   }

   StoragePoolPtr pool = getPoolUnlocked(poolId);
   if (!pool)
   {
      LOG(STORAGEPOOLS, ERR, "Request to add a target to a storage pool, which doesn't exist. ",
               targetId, poolId);

      return FhgfsOpsErr_UNKNOWNPOOL;
   }

   pool->addTarget(targetId, nodeId);

   return FhgfsOpsErr_SUCCESS;
}

/*
 * remove a target from the pool store (no matter in which pool it is)
 *
 * @param targetId
 */
StoragePoolId StoragePoolStore::removeTarget(uint16_t targetId)
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);

   for (auto iter = storagePools.begin(); iter != storagePools.end(); iter++)
   {
      StoragePoolPtr pool = iter->second;
      const bool rmRes = pool->removeTarget(targetId);

      if (rmRes) // target found; no need to search more pools
         return pool->getId();
   }

   return StoragePoolStore::INVALID_POOL_ID;
}

/*
 * add a buddy group ID to the pool store

 * @param buddyGroupId
 * @param poolId add to this pool id if buddyGroupId is inserted for the first time;
 * @param ignoreExisting if set to true it will not be considered an error if the buddyGroupId is
 *        already member of a pool; in this case the request is silently ignored
 * @return FhgfsOpsErr_SUCCESS if operation was succesful
 *         FhgfsOpsErr_EXISTS if buddy group is already member of a pool and !ignoreExisting
 *         FhgfsOpsErr_UNKNOWNPOOL if poolId doesn't belong to a valid pool
 */
FhgfsOpsErr StoragePoolStore::addBuddyGroup(uint16_t buddyGroupId, StoragePoolId poolId,
   bool ignoreExisting)
{
   // storage pools is only accessed in a read only way, but it gets locked in "write-mode". This
   // is, because it is technically possible, that two different threads try to add a buddy group
   // to two different pools at the same time and both could pass the "buddy group already exists"-
   // check and then both add the group. This is the easiest solution to prevent this, although
   // the storagePools map gets not modified
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);

   // first check if this buddy Group is in any of the pools already
   for (auto poolsIter = storagePools.begin(); poolsIter != storagePools.end(); poolsIter++)
   {
      if (poolsIter->second->hasBuddyGroup(buddyGroupId))
      {
         if (!ignoreExisting)
         {
            LOG(STORAGEPOOLS, ERR, "Requested to add buddy group to storage pool store, "
               "but buddy group is already member of a storage pool.", buddyGroupId,
               ("storagePoolId", poolsIter->second->getId()));

            return FhgfsOpsErr_EXISTS;
         }
         return FhgfsOpsErr_SUCCESS;
      }
   }

   StoragePoolPtr pool = getPoolUnlocked(poolId);
   if (!pool)
   {
      LOG(STORAGEPOOLS, ERR, "Request to add a buddy group to a storage pool, which doesn't exist.",
          buddyGroupId, poolId);

      return FhgfsOpsErr_UNKNOWNPOOL;
   }

   pool->addBuddyGroup(buddyGroupId);

   return FhgfsOpsErr_EXISTS;
}

/*
 * remove a buddy group ID from the pool store (no matter in which pool it is)
 *
 * intended to be used when a buddy group gets removed from the system
 *
 * @param buddyGroupId
 */
StoragePoolId StoragePoolStore::removeBuddyGroup(uint16_t buddyGroupId)
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);

   for (auto iter = storagePools.begin(); iter != storagePools.end(); iter++)
   {
      StoragePoolPtr pool = iter->second;
      const bool rmRes = pool->removeBuddyGroup(buddyGroupId);

      if (rmRes) // target found; no need to search more pools
         return pool->getId();
   }

   return StoragePoolStore::INVALID_POOL_ID;
}


/*
 * note: function has no locks, so the caller needs to hold a lock
 */
StoragePoolId StoragePoolStore::findFreeID() const
{
   for (StoragePoolId id = StoragePoolId(1); id <= MAX_POOL_ID; id++)
   {
      StoragePoolPtrMapCIter it = storagePools.find(id);

      if (it == storagePools.end())
         return id;
   }

   // no free ID found
   return INVALID_POOL_ID;
}

/*
 * @param id the ID of the storage pool to fetch
 *
 * @return a shared_ptr to the pool, if pool exists. If pool does not exist an empty pointer is
 * returned
 */
StoragePoolPtr StoragePoolStore::getPool(StoragePoolId id) const
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);

   return getPoolUnlocked(id);
}

/*
 * @param id the ID of the storage pool to fetch
 *
 * @return a shared_ptr to the pool, if pool exists. If pool does not exist an empty pointer is
 * returned
 */
StoragePoolPtr StoragePoolStore::getPoolUnlocked(StoragePoolId id) const
{
   try
   {
      StoragePoolPtr pool = storagePools.at(id);

      return pool;
   }
   catch (const std::out_of_range& e)
   {
      return {};
   }
}

/*
 * @param targetId a targetId to search for
 *
 * @return a shared_ptr to the pool, which contains the target (if any). If pool does not exist an
 * empty pointer is returned
 */
StoragePoolPtr StoragePoolStore::getPool(uint16_t targetId) const
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);

   for (StoragePoolPtrMapCIter it = storagePools.begin(); it != storagePools.end(); it++)
   {
      if (it->second->hasTarget(targetId))
      {
         return it->second;
      }
   }

   return {}; // = NULL
}


/*
 * get a vector with pointers to all pools
 */
StoragePoolPtrVec StoragePoolStore::getPoolsAsVec() const
{
   StoragePoolPtrVec outVec;

   RWLockGuard lock(rwlock, SafeRWLock_READ);

   for (StoragePoolPtrMapCIter it = storagePools.begin(); it != storagePools.end(); it++)
      outVec.push_back(it->second);

   return outVec;
}

bool StoragePoolStore::poolExists(StoragePoolId id) const
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);

   auto it = storagePools.find(id);

   return it != storagePools.end();
}

void StoragePoolStore::syncFromVector(const StoragePoolPtrVec& storagePoolVec)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);

   storagePools.clear();

   for (auto it = storagePoolVec.begin(); it != storagePoolVec.end(); it++)
      storagePools[(*it)->getId()] = *it;
}

size_t StoragePoolStore::getSize() const
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);

   return storagePools.size();
}

