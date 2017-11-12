#include "TargetMapper.h"

#include <common/app/log/LogContext.h>
#include <common/toolkit/MapTk.h>
#include <common/nodes/TargetStateInfo.h>
#include <common/threading/RWLockGuard.h>
#include <common/toolkit/ZipIterator.h>

TargetMapper::TargetMapper():
   states(NULL), storagePools(NULL), exceededQuotaStores(NULL), mappingsDirty(false) { }

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

   mappingsDirty = true;

   return { FhgfsOpsErr_SUCCESS, (oldSize != newSize) };
}

/**
 * @return true if the targetID was mapped
 */
bool TargetMapper::unmapTarget(uint16_t targetID)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   bool targetFound = false;

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

      mappingsDirty = true;
      targetFound = true;
   }

   safeLock.unlock(); // U N L O C K

   return targetFound;
}

/**
 * @return true if the nodeID was mapped at least once
 */
bool TargetMapper::unmapByNodeID(NumNodeID nodeID)
{
   bool elemsErased = false;

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

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

         mappingsDirty = true;
      }
      else
         iter++;
   }

   safeLock.unlock(); // U N L O C K

   return elemsErased;
}

/**
 * Applies the mapping from two separate lists (keys and values).
 */
void TargetMapper::syncTargetsFromLists(UInt16List& targetIDs, NumNodeIDList& nodeIDs)
{
   /* note: we pre-create a new map, then check if it is different from the current map and swap
      elements if the maps are different (to avoid long write-locking) */


   // pre-create new map

   TargetMap newTargets;

   for (ZipConstIterRange<UInt16List, NumNodeIDList> iter(targetIDs, nodeIDs);
        !iter.empty(); ++iter)
      newTargets[*iter()->first] = *iter()->second;

   // compare old and new targets map

   SafeRWLock safeCompareReadLock(&rwlock, SafeRWLock_READ); // L O C K (read)

   bool mapsEqual = true;

   if(unlikely(targets.size() != newTargets.size() ) )
      mapsEqual = false;
   else
   { // compare all keys and values
      TargetMapCIter targetsIter = targets.begin();
      TargetMapCIter newTargetsIter = newTargets.begin();

      for( ; targetsIter != targets.end(); targetsIter++, newTargetsIter++)
      {
         if(unlikely(
            (targetsIter->first != newTargetsIter->first) ||
            (targetsIter->second != newTargetsIter->second) ) )
         {
            mapsEqual = false;
            break;
         }
      }

   }

   safeCompareReadLock.unlock(); // U N L O C K (read)

   if(mapsEqual)
      return; // nothing has changed


   // update (swap) targets map

   SafeRWLock safeWriteLock(&rwlock, SafeRWLock_WRITE); // L O C K (write)

   targets.clear();
   targets.swap(newTargets);

   mappingsDirty = true;

   safeWriteLock.unlock(); // U N L O C K (write)

   SafeRWLock safePoolsUpdateReadLock(&rwlock, SafeRWLock_READ); // L O C K (read)

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

   safePoolsUpdateReadLock.unlock(); // U N L O C K (read)
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
 * Returns a copy of the internal targets map.
 *
 * Note: This method is expensive; if you just need to know to which nodeID a certain target is
 * mapped, use getNodeID() instead of this method.
 */
void TargetMapper::getMapping(TargetMap& outTargetMap) const
{
   RWLockGuard safeLock(rwlock, SafeRWLock_READ); // L O C K

   outTargetMap = targets;
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
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   this->states = states;

   safeLock.unlock(); // U N L O C K
}

/**
* Targets will automatically be added to the default storage pool when they are added to this store.
*/
void TargetMapper::attachStoragePoolStore(StoragePoolStore* storagePools)
{
   RWLockGuard(rwlock, SafeRWLock_WRITE);

   this->storagePools = storagePools;
}

/**
* Targets will automatically be added to the ExceededQuotaPerTarget stores when they are added to
* this store.
*/
void TargetMapper::attachExceededQuotaStores(ExceededQuotaPerTarget* exceededQuotaStores)
{
   RWLockGuard(rwlock, SafeRWLock_WRITE);

   this->exceededQuotaStores = exceededQuotaStores;
}

/**
 * Note: setStorePath must be called before using this.
 */
bool TargetMapper::loadFromFile()
{
   const char* logContext = "TargetMapper (load)";

   bool loaded = false;

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   if(!storePath.length() )
      goto unlock_and_exit;

   try
   {
      StringMap newTargetsStrMap;

      // load from file
      MapTk::loadStringMapFromFile(storePath.c_str(), &newTargetsStrMap);

      // apply loaded targets
      targets.clear();
      importFromStringMap(newTargetsStrMap);

      for (TargetMapCIter iter = targets.begin(); iter != targets.end(); iter++)
      {
         // add to attached states
         if (states)
         {
            states->addIfNotExists(iter->first,
               CombinedTargetState(TargetReachabilityState_POFFLINE, TargetConsistencyState_GOOD));
         }

         // add to attached storage pools
         // NOTE: only targets, which are not already present in a pool are added. This should usually
         // not happen, but is normal after an update from BeeGFS v6
         if (storagePools)
         {
            storagePools->addTarget(iter->first, iter->second,  StoragePoolStore::DEFAULT_POOL_ID,
                                    true);
         }

         // add to attached exceededQuotaStores
         if (exceededQuotaStores)
         {
            exceededQuotaStores->add(iter->first);
         }
      }

      mappingsDirty = true;

      loaded = true;
   }
   catch(InvalidConfigException& e)
   {
      LOG_DEBUG(logContext, Log_DEBUG, "Unable to open target mappings file: " + storePath + ". " +
         "SysErr: " + System::getErrString() );
      IGNORE_UNUSED_VARIABLE(logContext);
   }

unlock_and_exit:

   safeLock.unlock(); // U N L O C K

   return loaded;
}

/**
 * Note: setStorePath must be called before using this.
 *
 * @return true if file was successfully saved
 */
bool TargetMapper::saveToFile()
{
   const char* logContext = "TargetMapper (save)";

   RWLockGuard safeLock(rwlock, SafeRWLock_WRITE); // L O C K

   if(storePath.empty() )
      return false;

   try
   {
      StringMap targetsStrMap;

      exportToStringMap(targetsStrMap);

      MapTk::saveStringMapToFile(storePath.c_str(), &targetsStrMap);

      mappingsDirty = false;

      return true;
   }
   catch(InvalidConfigException& e)
   {
      LogContext(logContext).logErr("Unable to save target mappings file: " + storePath + ". " +
         "SysErr: " + System::getErrString() );
      IGNORE_UNUSED_VARIABLE(logContext);

      return false;
   }
}

/**
 * Note: unlocked, caller must hold lock.
 */
void TargetMapper::exportToStringMap(StringMap& outExportMap) const
{
   for(TargetMapCIter iter = targets.begin(); iter != targets.end(); iter++)
   {
      outExportMap[StringTk::uintToHexStr(iter->first) ] = iter->second.strHex();
   }
}

/**
 * Note: unlocked, caller must hold lock.
 * Note: the internal targets map is not cleared in this method.
 */
void TargetMapper::importFromStringMap(StringMap& importMap)
{
   for(StringMapIter iter = importMap.begin(); iter != importMap.end(); iter++)
   {
      targets[StringTk::strHexToUInt(iter->first) ] =
         NumNodeID(StringTk::strHexToUInt(iter->second) );
   }
}

