#include <common/threading/RWLockGuard.h>

#include "NodeCapacityPools.h"

/**
 * @param lowSpace only used for getPoolTypeFromFreeSpace()
 * @param emergencySpace only used for getPoolTypeFromFreeSpace()
 */
NodeCapacityPools::NodeCapacityPools(bool useDynamicPools,
      const DynamicPoolLimits& poolLimitsSpace, const DynamicPoolLimits& poolLimitsInodes) :
   poolLimitsSpace(poolLimitsSpace),
   poolLimitsInodes(poolLimitsInodes),
   pools(CapacityPool_END_DONTUSE)
{
   this->useDynamicPools = useDynamicPools;

   this->lastRoundRobinTarget = 0;
}

/**
 * Note: Unlocked, so caller must hold the write lock when calling this.
 *
 * @param keepPool targetID won't be removed from this particular pool
 */
void NodeCapacityPools::removeFromOthersUnlocked(uint16_t targetID,
   CapacityPoolType keepPool)
{
   /* note: targetID can only exist in one pool, so we can stop as soon as we erased an element from
      any pool */

   for(unsigned poolType=0;
       poolType < CapacityPool_END_DONTUSE;
       poolType++)
   {
      if(poolType == (unsigned)keepPool)
         continue;

      const size_t numErased = pools[poolType].erase(targetID);
      if(!numErased)
         continue;

      // targetID existed in this pool, so we're done
      return;
   }

}

/**
 * @return true if the target was either completely new or existed before but was moved to a
 * different pool now
 */
bool NodeCapacityPools::addOrUpdate(uint16_t targetID, CapacityPoolType poolType)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);

   /* note: map::insert().second is true if the key was inserted and false if it already existed
      (so we need to check other pools only if it was really inserted) */

   const bool newInsertion = pools[poolType].insert(targetID).second;

   if(newInsertion)
      removeFromOthersUnlocked(targetID, poolType);

   return newInsertion;
}

/**
 * Note: Adds the targetID only if is doesn't exist in any of the pools (not only the specified
 * one).
 *
 * @return true if this is a new target, false if the target existed already
 */
bool NodeCapacityPools::addIfNotExists(uint16_t targetID, CapacityPoolType poolType)
{
   /* this is called often with already existing targets (e.g. for every mds heartbeat), so we have
    a quick path with only a read lock */

   bool needUpdate = true; // whether or not we need the slow path to update something

   { // lock scope
      // quick read lock check-only path
      RWLockGuard readLock(rwlock, SafeRWLock_READ);

      for (unsigned currentPoolType = 0; currentPoolType < CapacityPool_END_DONTUSE;
         currentPoolType++)
      {
         if (pools[currentPoolType].find(targetID) == pools[currentPoolType].end())
            continue; // not found in this pool

         // target found in this pool

         needUpdate = false;

         break;
      }
   }

   if (!needUpdate)
      return false;

   { // lock scope
     // slow write lock path (because target wasn't found in quick path)
      RWLockGuard writeLock(rwlock, SafeRWLock_WRITE);

      // recheck existence before adding, because it might have been added after read lock release

      needUpdate = true;

      for (unsigned currentPoolType = 0; currentPoolType < CapacityPool_END_DONTUSE;
         currentPoolType++)
      {
         if (pools[currentPoolType].find(targetID) == pools[currentPoolType].end())
            continue; // not found in this pool

         // target found in this pool

         needUpdate = false;

         break;
      }

      if (needUpdate)
         pools[poolType].insert(targetID);

      return needUpdate;
   }
}


void NodeCapacityPools::remove(uint16_t targetID)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);

   for(unsigned currentPoolType = 0;
      currentPoolType < CapacityPool_END_DONTUSE;
      currentPoolType++)
   {
      const size_t numErased = pools[currentPoolType].erase(targetID);
      if(!numErased)
         continue;

      break;
   }
}

/*
 * @param lists the capacity pool lists in a vector with mappings lists[CapacityPoolType]
 */
void NodeCapacityPools::syncPoolsFromLists(const UInt16ListVector& lists)
{
   // we fill temporary sets first without lock, then swap with lock (to keep write-lock time short)
   UInt16SetVector newPools(CapacityPool_END_DONTUSE);

   for(unsigned currentPoolType=0;
      currentPoolType < CapacityPool_END_DONTUSE;
      currentPoolType++)
   {
      newPools[currentPoolType].insert(lists[currentPoolType].begin(),
                                       lists[currentPoolType].end());
   }

   // temporary sets are ready, now swap with internal sets
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);
   newPools.swap(pools);
}

/**
 * Note: More efficient than the list version, because this will swap directly.
 *
 * @param sets the capacity pool sets in a vector with mappings lists[CapacityPoolType]; don't
 *        use contents after calling this!
 */
void NodeCapacityPools::syncPoolsFromSets(UInt16SetVector& sets)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);

   sets.swap(pools);
}

/**
 * @return the capacity pool lists in a vector with mappings lists[CapacityPoolType]
 */
UInt16ListVector NodeCapacityPools::getPoolsAsLists() const
{
   UInt16ListVector outPools(CapacityPool_END_DONTUSE);

   RWLockGuard lock(rwlock, SafeRWLock_READ);

   for(unsigned currentPoolType=0;
      currentPoolType < CapacityPool_END_DONTUSE;
      currentPoolType++)
   {
      for(UInt16SetCIter iter = pools[currentPoolType].begin();
          iter != pools[currentPoolType].end();
          iter++)
      {
         /* note: build-in multi-element list::insert is inefficient (walks list two times), thus
             we do the copy ourselves here */
         outPools[currentPoolType].push_back(*iter);
      }
   }
   return outPools;
}

/**
 * @param numTargets number of desired targets
 * @param minNumRequiredTargets the minimum number of targets the caller needs; ususally 1
 * for RAID-0 striping and 2 for mirroring (so this parameter is intended to avoid problems when
 * there is only 1 target left in the normal pool and the user has mirroring turned on).
 * @param preferredTargets may be NULL
 */
void NodeCapacityPools::chooseStorageTargets(unsigned numTargets, unsigned minNumRequiredTargets,
   const UInt16List* preferredTargets, UInt16Vector* outTargets)
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);

   if(!preferredTargets || preferredTargets->empty() )
   { // no preference => start with first pool that contains any targets
      if (!pools[CapacityPool_NORMAL].empty())
      {
         chooseStorageNodesNoPref(pools[CapacityPool_NORMAL], numTargets, outTargets);

         if(outTargets->size() >= minNumRequiredTargets)
            return;
      }

      /* note: no "else if" here, because we want to continue with next pool if we didn't find
         enough targets for minNumRequiredTargets in previous pool */

      if (!pools[CapacityPool_LOW].empty())
      {
         chooseStorageNodesNoPref(pools[CapacityPool_LOW], numTargets - outTargets->size(),
                                  outTargets);

         if(outTargets->size() >= minNumRequiredTargets)
            return;
      }

      chooseStorageNodesNoPref(pools[CapacityPool_EMERGENCY], numTargets, outTargets);
   }
   else
   {
      // caller has preferred targets, so we can't say a priori whether nodes will be found or not
      // in a pool. our strategy here is to automatically allow non-preferred targets before
      // touching the emergency pool.

      std::set<uint16_t> chosenTargets;

      // try normal and low pool with preferred targets...

      chooseStorageNodesWithPref(pools[CapacityPool_NORMAL], numTargets, preferredTargets, false,
                                 outTargets, chosenTargets);

      if(outTargets->size() >= minNumRequiredTargets)
         return;

      chooseStorageNodesWithPref(pools[CapacityPool_LOW], numTargets - outTargets->size(),
                                 preferredTargets, false, outTargets, chosenTargets);

      if(!outTargets->empty() )
         return;

      /* note: currently, we cannot just continue here with non-empty outTargets (even if
         "outTargets->size() < minNumRequiredTargets"), because we would need a mechanism to exclude
         the already chosen outTargets for that (e.g. like an inverted preferredTargets list). */

      // no targets yet - allow non-preferred targets before using emergency pool...

      chooseStorageNodesWithPref(pools[CapacityPool_NORMAL], numTargets, preferredTargets, true,
                                 outTargets, chosenTargets);

      if(outTargets->size() >= minNumRequiredTargets)
         return;

      chooseStorageNodesWithPref(pools[CapacityPool_LOW], numTargets - outTargets->size(),
                                 preferredTargets, true, outTargets, chosenTargets);

      if(!outTargets->empty() )
         return;

      /* still no targets available => we have to try the emergency pool (first with preference,
         then without preference) */

      chooseStorageNodesWithPref(pools[CapacityPool_EMERGENCY], numTargets, preferredTargets, false,
         outTargets, chosenTargets);
      if(!outTargets->empty() )
         return;

      chooseStorageNodesWithPref(pools[CapacityPool_EMERGENCY], numTargets, preferredTargets, true,
         outTargets, chosenTargets);
   }
}

/**
 * Alloc storage targets in a round-robin fashion.
 *
 * Disadvantages:
 *  - no support for preferred targets
 *     - would be really complex for multiple clients with distinct preferred targets and per-client
 *       lastTarget wouldn't lead to perfect balance)
 *  - requires write lock
 *  - lastTarget is not on a per-pool basis and hence
 *       - would not be perfectly round-robin when targets are moved to other pools
 *       - doesn't work with minNumRequiredTargets as switching pools would mess with the global
 *         lastTarget value
 *  - lastTarget is not saved across server restart
 *
 * ...so this should only be used in special scenarios and not as a general replacement for the
 * normal chooseStorageTargets() method.
 */
void NodeCapacityPools::chooseStorageTargetsRoundRobin(unsigned numTargets,
   UInt16Vector* outTargets)
{
   RWLockGuard lock(rwlock, SafeRWLock_WRITE);

   // no preference settings supported => use first pool that contains any targets
   if (!pools[CapacityPool_NORMAL].empty())
      chooseStorageNodesNoPrefRoundRobin(pools[CapacityPool_NORMAL], numTargets, outTargets);
   else if (!pools[CapacityPool_LOW].empty())
      chooseStorageNodesNoPrefRoundRobin(pools[CapacityPool_LOW], numTargets, outTargets);
   else
      chooseStorageNodesNoPrefRoundRobin(pools[CapacityPool_EMERGENCY], numTargets, outTargets);
}


/**
 * Note: Unlocked (=> caller must hold read lock)
 *
 * @param outTargets might cotain less than numTargets if not enough targets are known
 */
void NodeCapacityPools::chooseStorageNodesNoPref(UInt16Set& activeTargets,
   unsigned numTargets, UInt16Vector* outTargets)
{
   // note: we use the name "activeTargets" for the pool here to keep the code very similar to the
      // nodes chooser in the NodeStore class
   if(activeTargets.empty() )
      return; // there's nothing we can do without any storage targets

   const unsigned activeTargetsSize = activeTargets.size();

   if(numTargets > activeTargetsSize)
      numTargets = activeTargetsSize;

   UInt16SetCIter iter;
   const int partSize = activeTargetsSize / numTargets;

   moveIterToRandomElem(activeTargets, iter);

   outTargets->reserve(numTargets);

   // for each target in numTargets
   for(unsigned i=0; i < numTargets; i++)
   {
      const int rangeMin = i*partSize;
      const int rangeMax = (i==(numTargets-1) ) ? (activeTargetsSize-1) : (rangeMin + partSize - 1);
      // rangeMax decision because numTargets might not devide activeTargetsSize without a remainder

      int next = randGen.getNextInRange(rangeMin, rangeMax);

      /* move iter to the chosen target, add it to outTargets, and skip the remaining targets of
         this part */

      for(int j=rangeMin; j < next; j++)
         moveIterToNextRingElem(activeTargets, iter);

      outTargets->push_back(*iter);

      for(int j=next; j < (rangeMax+1); j++)
         moveIterToNextRingElem(activeTargets, iter);
   }

}

/**
 * Note: Unlocked (=> caller must hold write lock)
 *
 * @param outTargets might cotain less than numTargets if not enough targets are known
 */
void NodeCapacityPools::chooseStorageNodesNoPrefRoundRobin(UInt16Set& activeTargets,
   unsigned numTargets, UInt16Vector* outTargets)
{
   // note: we use the name "activeTargets" for the pool here to keep the code very similar to the
      // nodes chooser in the NodeStore class

   if(activeTargets.empty() )
      return; // there's nothing we can do without any storage targets

   const unsigned activeTargetsSize = activeTargets.size();

   if(numTargets > activeTargetsSize)
      numTargets = activeTargetsSize;

   UInt16SetCIter iter = activeTargets.lower_bound(lastRoundRobinTarget);
   if(iter == activeTargets.end() )
      iter = activeTargets.begin();

   outTargets->reserve(numTargets);

   // just add the requested number of targets sequentially from the set
   for(unsigned i=0; i < numTargets; i++)
   {
      moveIterToNextRingElem(activeTargets, iter);
      outTargets->push_back(*iter);
   }

   lastRoundRobinTarget = *iter;
}


/**
 * Note: Unlocked (=> caller must hold read lock)
 *
 * Note: This method assumes that there really is at least one preferred target and that those
 * targets are probably active. So do not use it as a replacement for the version without preferred
 * targets!
 *
 * @param allowNonPreferredTargets true to enable use of non-preferred targets if not enough
 *    preferred targets are active to satisfy numTargets
 * @param outTargets might cotain less than numTargets if not enough targets are known
 */
void NodeCapacityPools::chooseStorageNodesWithPref(UInt16Set& activeTargets,
   unsigned numTargets, const UInt16List* preferredTargets, bool allowNonPreferredTargets,
   UInt16Vector* outTargets, std::set<uint16_t>& chosenTargets)
{
   // note: we use the name "activeTargets" for the pool here to keep the code very similar to the
      // nodes chooser in the NodeStore class

   if(activeTargets.empty() )
      return; // there's nothing we can do without any storage targets

   const unsigned activeTargetsSize = activeTargets.size();

   // max number of targets is limited by the number of known active targets
   if(numTargets > activeTargetsSize)
      numTargets = activeTargetsSize;

   // Stage 1: add all the preferred targets that are actually available to the outTargets

   // note: we use a separate set for the outTargets here to quickly find out (in stage 2) whether
   // we already added a certain node from the preferred targets (in stage 1)

   UInt16ListConstIter preferredIter;
   UInt16SetIter activeTargetsIter; // (will be re-used in stage 2)

   moveIterToRandomElem(*preferredTargets, preferredIter);

   outTargets->reserve(numTargets);

   // walk over all the preferred targets and add them to outTargets when they are available
   // (note: iterTmp is used to avoid calling preferredTargets->size() )
   for(UInt16ListConstIter iterTmp = preferredTargets->begin();
       (iterTmp != preferredTargets->end() ) && numTargets;
       iterTmp++)
   {
      activeTargetsIter = activeTargets.find(*preferredIter);

      if(activeTargetsIter != activeTargets.end() && chosenTargets.insert(*preferredIter).second)
      { // this preferred node is active => add to outTargets
         outTargets->push_back(*preferredIter);
         numTargets--;
      }

      moveIterToNextRingElem(*preferredTargets, preferredIter);
   }

   // Stage 2: add the remaining requested number of targets from the active targets

   // if numTargets is greater than 0 then we have some requested targets left, that could not be
   // taken from the preferred targets

   // we keep it simple here, because usually there will be enough preferred targets available,
   // so that this case is quite unlikely

   if(allowNonPreferredTargets && numTargets)
   {
      UInt16SetIter outTargetsSetIter;

      moveIterToRandomElem(activeTargets, activeTargetsIter);

      // while we haven't found the number of requested targets
      while(numTargets)
      {
         if(chosenTargets.insert(*activeTargetsIter).second)
         {
            outTargets->push_back(*activeTargetsIter);
            numTargets--;
         }

         moveIterToNextRingElem(activeTargets, activeTargetsIter);
      }
   }

}

/**
 * get the pool type to which a nodeID is currently assigned.
 *
 * @return false if nodeID not found.
 */
bool NodeCapacityPools::getPoolAssignment(uint16_t nodeID, CapacityPoolType* outPoolType) const
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);

   *outPoolType = CapacityPool_EMERGENCY;

   for(unsigned poolType=0;
       poolType < CapacityPool_END_DONTUSE;
       poolType++)
   {
      UInt16SetCIter iter = pools[poolType].find(nodeID);

      if(iter != pools[poolType].end() )
      {
         *outPoolType = (CapacityPoolType)poolType;

         return true;
      }
   }

   return false;
}

/**
 * Return internal state as human-readable string.
 *
 * Note: Very expensive, only intended for debugging.
 */
std::string NodeCapacityPools::getStateAsStr() const
{
   RWLockGuard lock(rwlock, SafeRWLock_READ);

   std::ostringstream stateStream;

   stateStream << " lastRoundRobinTarget: " << lastRoundRobinTarget << std::endl;
   stateStream << " lowSpace: " << this->poolLimitsSpace.getLowLimit() << std::endl;
   stateStream << " emergencySpace: " << this->poolLimitsSpace.getEmergencyLimit() << std::endl;
   stateStream << " lowInodes: " << this->poolLimitsInodes.getLowLimit() << std::endl;
   stateStream << " emergencyInodes: " << this->poolLimitsInodes.getEmergencyLimit() << std::endl;
   stateStream << " dynamicLimits: " << (this->useDynamicPools ? "on" : "off") << std::endl;

   if(this->useDynamicPools)
   {
      stateStream << "  normalSpaceSpreadThreshold: " <<
            this->poolLimitsSpace.getNormalSpreadThreshold() << std::endl;
      stateStream << "  lowSpaceSpreadThreshold: " <<
            this->poolLimitsSpace.getLowSpreadThreshold() << std::endl;
      stateStream << "  lowSpaceDynamicLimit: " <<
            this->poolLimitsSpace.getLowDynamicLimit() << std::endl;
      stateStream << "  emergencySpaceDynamicLimit: " <<
            this->poolLimitsSpace.getEmergencyDynamicLimit() << std::endl;

      stateStream << "  normalInodesSpreadThreshold: " <<
            this->poolLimitsInodes.getNormalSpreadThreshold() << std::endl;
      stateStream << "  lowInodesSpreadThreshold: " <<
            this->poolLimitsInodes.getLowSpreadThreshold() << std::endl;
      stateStream << "  lowInodesDynamicLimit: " <<
            this->poolLimitsInodes.getLowDynamicLimit() << std::endl;
      stateStream << "  emergencyInodesDynamicLimit: " <<
            this->poolLimitsInodes.getEmergencyDynamicLimit() << std::endl;
   }

   stateStream << std::endl;

   for(unsigned poolType=0;
       poolType < CapacityPool_END_DONTUSE;
       poolType++)
   {
      stateStream << " pool " << poolTypeToStr( (CapacityPoolType) poolType) << ":" <<
         std::endl;

      for(UInt16SetCIter iter = pools[poolType].begin();
          iter != pools[poolType].end();
          iter++)
      {
         stateStream << "  " << *iter;
      }

      stateStream << std::endl;
   }

   return stateStream.str();
}

/**
 * @return pointer to static string buffer (no free needed)
 */
const char* NodeCapacityPools::poolTypeToStr(CapacityPoolType poolType)
{
   switch(poolType)
   {
      case CapacityPool_NORMAL:
      {
         return "Normal";
      } break;

      case CapacityPool_LOW:
      {
         return "Low";
      } break;

      case CapacityPool_EMERGENCY:
      {
         return "Emergency";
      } break;

      default:
      {
         return "Unknown/Invalid";
      } break;
   }
}
