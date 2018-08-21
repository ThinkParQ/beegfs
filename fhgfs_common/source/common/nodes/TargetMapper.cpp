#include <common/app/log/LogContext.h>
#include <common/toolkit/MapTk.h>
#include <common/nodes/TargetStateInfo.h>
#include <common/toolkit/ZipIterator.h>
#include "TargetMapper.h"


TargetMapper::TargetMapper()
   : capacityPools(NULL), states(NULL), mappingsDirty(false)
{ }

/**
 * Note: re-maps targetID if it was mapped before.
 *
 * @return true if the targetID was not mapped before
 */
bool TargetMapper::mapTarget(uint16_t targetID, NumNodeID nodeID)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   size_t oldSize = targets.size();

   targets[targetID] = nodeID;

   size_t newSize = targets.size();

   if(capacityPools && (oldSize != newSize) )
      capacityPools->addIfNotExists(targetID, nodeID, CapacityPool_LOW);

   if(states && (oldSize != newSize) )
      states->addIfNotExists(targetID, CombinedTargetState(
         TargetReachabilityState_POFFLINE, TargetConsistencyState_GOOD) );

   mappingsDirty = true;

   safeLock.unlock(); // U N L O C K

   return (oldSize != newSize);
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

      if(capacityPools)
         capacityPools->remove(targetID);

      if(states)
         states->removeTarget(targetID);

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

         if(capacityPools)
            capacityPools->remove(targetID);


         if(states)
            states->removeTarget(targetID);

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
 *
 * Note: Does add, but not remove targets from attached capacity pools. (Removal is not impl'ed to
 * save the time for finding out which targets are gone - that would involve list comparison.)
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


   // update attached capacity pool
   /* (note: this case is especially important during metaserver init to fill the capacity pools
      instead of waiting for InternodeSyncer to do it) */

   SafeRWLock safePoolsUpdateReadLock(&rwlock, SafeRWLock_READ); // L O C K (read)

   if(capacityPools)
   {
      for(TargetMapCIter iter = targets.begin(); iter != targets.end(); iter++)
         capacityPools->addIfNotExists(iter->first, iter->second, CapacityPool_LOW);
   }

   // add to attached states
   if ( states )
   {
      for ( TargetMapCIter iter = targets.begin(); iter != targets.end(); iter++ )
         states->addIfNotExists(iter->first, CombinedTargetState() );
   }

   safePoolsUpdateReadLock.unlock(); // U N L O C K (read)
}

/**
 * Returns the mapping in two separate lists (keys and values).
 */
void TargetMapper::getMappingAsLists(UInt16List& outTargetIDs, NumNodeIDList& outNodeIDs)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   for(TargetMapIter iter = targets.begin(); iter != targets.end(); iter++)
   {
      outTargetIDs.push_back(iter->first);
      outNodeIDs.push_back(iter->second);
   }

   safeLock.unlock(); // U N L O C K
}

/**
 * Returns a copy of the internal targets map.
 *
 * Note: This method is expensive; if you just need to know to which nodeID a certain target is
 * mapped, use getNodeID() instead of this method.
 */
void TargetMapper::getMapping(TargetMap& outTargetMap)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   outTargetMap = targets;

   safeLock.unlock(); // U N L O C K
}

/**
 * Returns a list of targetIDs which are mapped to the given nodeID.
 */
void TargetMapper::getTargetsByNode(NumNodeID nodeID, UInt16List& outTargetIDs)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   for(TargetMapIter iter = targets.begin(); iter != targets.end(); iter++)
   {
      if (iter->second == nodeID)
      {
         outTargetIDs.push_back(iter->first);
      }
   }

   safeLock.unlock(); // U N L O C K
}

/**
 * Targets will automatically be added/removed from attached capacity pools when they are
 * added/removed from this store.
 */
void TargetMapper::attachCapacityPools(TargetCapacityPools* capacityPools)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   this->capacityPools = capacityPools;

   safeLock.unlock(); // U N L O C K
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

      // add to attached capacity pools
      if(capacityPools)
      {
         for(TargetMapCIter iter = targets.begin(); iter != targets.end(); iter++)
            capacityPools->addIfNotExists(iter->first, iter->second, CapacityPool_LOW);
      }

      // add to attached states
      if ( states )
      {
         const CombinedTargetState defaultState(TargetReachabilityState_POFFLINE,
               TargetConsistencyState_GOOD);
         CombinedTargetState ignoredReturnState;
         for ( TargetMapCIter iter = targets.begin(); iter != targets.end(); iter++ )
         {
            if (!states->getState(iter->first, ignoredReturnState)) {
               LogContext(logContext).log(Log_WARNING,
                     "Storage target " + StringTk::intToStr(iter->first)
                     + " missing in targetStates. Consistency state is lost.");
               LogContext(logContext).log(Log_WARNING, "Adding default state for storage target "
                     + StringTk::intToStr(iter->first) + ": "
                     + states->stateToStr(defaultState));
               IGNORE_UNUSED_VARIABLE(logContext);

               states->addIfNotExists(iter->first, defaultState);
            }
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
 */
bool TargetMapper::saveToFile()
{
   const char* logContext = "TargetMapper (save)";

   bool saved = false;

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   if(storePath.empty() )
      goto unlock_and_exit;


   try
   {
      StringMap targetsStrMap;

      exportToStringMap(targetsStrMap);

      MapTk::saveStringMapToFile(storePath.c_str(), &targetsStrMap);

      mappingsDirty = false;

      saved = true;
   }
   catch(InvalidConfigException& e)
   {
      LogContext(logContext).logErr("Unable to save target mappings file: " + storePath + ". " +
         "SysErr: " + System::getErrString() );
      IGNORE_UNUSED_VARIABLE(logContext);
   }

unlock_and_exit:

   safeLock.unlock(); // U N L O C K

   return saved;
}

/**
 * Note: unlocked, caller must hold lock.
 */
void TargetMapper::exportToStringMap(StringMap& outExportMap)
{
   for(TargetMapIter iter = targets.begin(); iter != targets.end(); iter++)
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

