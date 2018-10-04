#include "TargetMapper.h"

#include <common/app/log/LogContext.h>
#include <common/nodes/TargetStateInfo.h>
#include <common/threading/RWLockGuard.h>
#include <common/toolkit/ZipIterator.h>

TargetMapper::TargetMapper():
   states(NULL), storagePools(NULL), exceededQuotaStores(NULL) { }

/**
 * Note: re-maps targetID if it was mapped before.
 *
 * @param storagePoolId if the target is new it gets added to this storagePoolId (note: might be
 *        invalid pool, in which case storage pool store has to handle this)
 *
 * @return pair of <FhgfsOpsErr, bool>, with first indicating if any error occured and second
 *         indicating if the target was newly added (in which case true is returned)
 */
std::pair<FhgfsOpsErr, bool> TargetMapper::mapTarget(uint16_t targetID, NumNodeID nodeID,
      StoragePoolId storagePoolId)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);

   size_t oldSize = targets.size();

   targets[targetID] = nodeID;

   size_t newSize = targets.size();

   if (oldSize != newSize) // new target
   {
      if (storagePools)
      {
         FhgfsOpsErr addRes = storagePools->addTarget(targetID, nodeID, storagePoolId, true);
         if (addRes != FhgfsOpsErr_SUCCESS) // target could not be mapped to storagePoolId
         { // => unmap target again and return an error
            targets.erase(targetID);

            return { addRes, false };
         }
      }

      if (states)
         states->addIfNotExists(targetID,
            CombinedTargetState(TargetReachabilityState_POFFLINE, TargetConsistencyState_GOOD));

      if (exceededQuotaStores)
         exceededQuotaStores->add(targetID, false);
   }

   return { FhgfsOpsErr_SUCCESS, (oldSize != newSize) };
}

/**
 * @return true if the targetID was mapped
 */
bool TargetMapper::unmapTarget(uint16_t targetID)
{
   RWLockGuard const lock(rwlock, SafeRWLock_WRITE);

   TargetMapIter iter = targets.find(targetID);
   if(iter != targets.end() )
   { // targetID found

      targets.erase(iter);

      if (storagePools)
         storagePools->removeTarget(targetID);

      if(states)
         states->removeTarget(targetID);

      if (exceededQuotaStores)
         exceededQuotaStores->remove(targetID);

      return true;
   }

   return false;
}

/**
 * @return true if the nodeID was mapped at least once
 */
bool TargetMapper::unmapByNodeID(NumNodeID nodeID)
{
   bool elemsErased = false;

   RWLockGuard const lock(rwlock, SafeRWLock_WRITE);

   for(TargetMapIter iter = targets.begin(); iter != targets.end(); /* iter inc'ed inside loop */)
   {
      if(iter->second == nodeID)
      {
         uint16_t targetID(iter->first);

         TargetMapIter eraseIter(iter); // save current iter pos
         iter++; // move iter to next elem

         targets.erase(eraseIter);

         if (storagePools)
            storagePools->removeTarget(targetID);

         if(states)
            states->removeTarget(targetID);

         if (exceededQuotaStores)
            exceededQuotaStores->remove(targetID);

         elemsErased = true;
      }
      else
         iter++;
   }

   return elemsErased;
}

void TargetMapper::syncTargets(TargetMap newTargets)
{
   {
      RWLockGuard const lock(rwlock, SafeRWLock_WRITE);

      targets.swap(newTargets);
   }

   {
      RWLockGuard const lock(rwlock, SafeRWLock_READ);

      for (TargetMapCIter iter = targets.begin(); iter != targets.end(); iter++)
      {
         // add to attached states
         if (states)
         {
            states->addIfNotExists(iter->first, CombinedTargetState());
         }

         if (exceededQuotaStores)
         {
            exceededQuotaStores->add(iter->first);
         }
      }
   }
}

/**
 * Returns the mapping in two separate lists (keys and values).
 */
void TargetMapper::getMappingAsLists(UInt16List& outTargetIDs, NumNodeIDList& outNodeIDs) const
{
   RWLockGuard safeLock(rwlock, SafeRWLock_READ); // L O C K

   for(TargetMapCIter iter = targets.begin(); iter != targets.end(); iter++)
   {
      outTargetIDs.push_back(iter->first);
      outNodeIDs.push_back(iter->second);
   }
}

/**
 * Returns a list of targetIDs which are mapped to the given nodeID.
 */
void TargetMapper::getTargetsByNode(NumNodeID nodeID, UInt16List& outTargetIDs) const
{
   RWLockGuard safeLock(rwlock, SafeRWLock_READ); // L O C K

   for(TargetMapCIter iter = targets.begin(); iter != targets.end(); iter++)
   {
      if (iter->second == nodeID)
      {
         outTargetIDs.push_back(iter->first);
      }
   }
}

/**
* Targets will automatically be added/removed from attached target states when they are
* added/removed from this store.
*/
void TargetMapper::attachStateStore(TargetStateStore* states)
{
   RWLockGuard const lock(rwlock, SafeRWLock_WRITE);

   this->states = states;
}

/**
* Targets will automatically be added to the default storage pool when they are added to this store.
*/
void TargetMapper::attachStoragePoolStore(StoragePoolStore* storagePools)
{
   RWLockGuard const _(rwlock, SafeRWLock_WRITE);

   this->storagePools = storagePools;
}

/**
* Targets will automatically be added to the ExceededQuotaPerTarget stores when they are added to
* this store.
*/
void TargetMapper::attachExceededQuotaStores(ExceededQuotaPerTarget* exceededQuotaStores)
{
   RWLockGuard const _(rwlock, SafeRWLock_WRITE);

   this->exceededQuotaStores = exceededQuotaStores;
}
