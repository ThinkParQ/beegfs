#include <common/app/log/LogContext.h>
#include <common/toolkit/MapTk.h>
#include "TargetMapper.h"


TargetMapper::TargetMapper()
{
   mappingsDirty = false;
   capacityPools = NULL;
}

/**
 * Note: re-maps targetID if it was mapped before.
 *
 * @return true if the targetID was not mapped before
 */
bool TargetMapper::mapTarget(uint16_t targetID, uint16_t nodeID)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   size_t oldSize = targets.size();

   targets[targetID] = nodeID;

   size_t newSize = targets.size();

   if(capacityPools && (oldSize != newSize) )
      capacityPools->addIfNotExists(targetID, TargetCapacityPool_LOW);

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

   size_t numErased = targets.erase(targetID);

   if(capacityPools && numErased)
      capacityPools->remove(targetID);

   if(numErased)
      mappingsDirty = true;

   safeLock.unlock(); // U N L O C K

   return (numErased != 0);
}

/**
 * @return true if the nodeID was mapped at least once
 */
bool TargetMapper::unmapByNodeID(uint16_t nodeID)
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
void TargetMapper::syncTargetsFromLists(UInt16List& targetIDs, UInt16List& nodeIDs)
{
   // note: we pre-create a new map and then swap elements to avoid long write-locking

   TargetMap newTargets;
   UInt16ListConstIter keysIter = targetIDs.begin();
   UInt16ListConstIter valuesIter = nodeIDs.begin();

   for( ; keysIter != targetIDs.end(); keysIter++, valuesIter++)
      newTargets[*keysIter] = *valuesIter;


   SafeRWLock safeWriteLock(&rwlock, SafeRWLock_WRITE); // L O C K (write)

   targets.clear();
   targets.swap(newTargets);

   mappingsDirty = true;

   safeWriteLock.unlock(); // U N L O C K (write)


   // add to attached capacity pool
   /* (note: this case is especially important during metaserver init to fill the capacity pools
      instead of waiting for InternodeSyncer to do it) */

   SafeRWLock safeReadLock(&rwlock, SafeRWLock_READ); // L O C K (read)

   if(capacityPools)
   {
      for(TargetMapCIter iter = targets.begin(); iter != targets.end(); iter++)
         capacityPools->addIfNotExists(iter->first, TargetCapacityPool_LOW);
   }

   safeReadLock.unlock(); // U N L O C K (read)
}

/**
 * Returns the mapping in two separate lists (keys and values).
 */
void TargetMapper::getMappingAsLists(UInt16List& outTargetIDs, UInt16List& outNodeIDs)
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
 * Returns a list of targetIDs which are mapped to the given nodeID.
 */
void TargetMapper::getTargetsByNode(uint16_t nodeID, UInt16List& outTargetIDs)
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
            capacityPools->addIfNotExists(iter->first, TargetCapacityPool_LOW);
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
      outExportMap[StringTk::uintToHexStr(iter->first) ] =
         StringTk::uintToHexStr(iter->second);
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
         StringTk::strHexToUInt(iter->second);
   }
}

