#include "TargetCapacityPools.h"

/**
 * @param lowSpace only used for getPoolTypeFromFreeSpace()
 * @param emergencySpace only used for getPoolTypeFromFreeSpace()
 */
TargetCapacityPools::TargetCapacityPools(int64_t lowSpace, int64_t emergencySpace)
{
   this->lowSpace = lowSpace;
   this->emergencySpace = emergencySpace;

   this->lastRoundRobinTarget = 0;
}

/**
 * Note: Unlocked, so caller must hold the write lock when calling this.
 *
 * Note: Adds without checking whether the targetID exists in another pool, so caller must make sure
 * that this isn't the case before calling.
 */
void TargetCapacityPools::addUncheckedUnlocked(uint16_t targetID,
   TargetCapacityPoolType poolType)
{
   if(poolType == TargetCapacityPool_NORMAL)
   {
      poolNormal.insert(targetID);
   }
   else
   if(poolType == TargetCapacityPool_LOW)
   {
      poolLow.insert(targetID);
   }
   else
   { // emergency pool
      poolEmergency.insert(targetID);
   }

}

/**
 * Note: Unlocked, so caller must hold the write lock when calling this.
 *
 * @param keepPool targetID won't be removed from this particular pool
 */
void TargetCapacityPools::removeFromOthersUnlocked(uint16_t targetID,
   TargetCapacityPoolType keepPool)
{
   // note: targetID can only exist in one pool, so we can stop as soon as we erased an element from
   // any pool

   if(keepPool != TargetCapacityPool_NORMAL)
   {
      if(poolNormal.erase(targetID) )
         return;
   }

   if(keepPool != TargetCapacityPool_LOW)
   {
      if(poolLow.erase(targetID) )
         return;
   }

   if(keepPool != TargetCapacityPool_EMERGENCY)
   {
      if(poolEmergency.erase(targetID) )
         return;
   }

}

UInt16Set* TargetCapacityPools::getPoolFromTypeUnlocked(TargetCapacityPoolType poolType)
{
   if(poolType == TargetCapacityPool_NORMAL)
      return &poolNormal;

   if(poolType == TargetCapacityPool_LOW)
      return &poolLow;

   return &poolEmergency;
}

/**
 * @return true if the target was either completely new or existed before but was moved to a
 * different pool now
 */
bool TargetCapacityPools::addOrUpdate(uint16_t targetID, TargetCapacityPoolType poolType)
{
   SafeRWLock lock(&rwlock, SafeRWLock_WRITE);

   UInt16Set* pool = getPoolFromTypeUnlocked(poolType);

   // note: map::insert().second is true if the key was inserted and false if it already existed
      // (so we need to check other pools only if it was really inserted)

   bool newInsertion = pool->insert(targetID).second;

   if(newInsertion)
      removeFromOthersUnlocked(targetID, poolType);

   lock.unlock();

   return newInsertion;
}

/**
 * Note: Adds the targetID only if is doesn't exist in any of the pools (not only the specified
 * one).
 *
 * @return true if this is a new target, false if the target existed already
 */
bool TargetCapacityPools::addIfNotExists(uint16_t targetID, TargetCapacityPoolType poolType)
{
   /* this is called often with already existing targets (e.g. for every mds heartbeat), so we have
      a quick path with only a read lock */

   // quick read lock check-only path

   SafeRWLock readLock(&rwlock, SafeRWLock_READ); // L O C K read

   bool foundTarget = false;

   if( (poolNormal.find(targetID) != poolNormal.end() ) ||
       (poolLow.find(targetID) != poolLow.end() ) ||
       (poolEmergency.find(targetID) != poolEmergency.end() ) )
   { // found target => we're done
      foundTarget = true;
   }

   readLock.unlock(); // U N L O C K read

   if(foundTarget)
      return false;

   // slow write lock path (because target wasn't found in quick path)

   SafeRWLock writeLock(&rwlock, SafeRWLock_WRITE); // L O C K write

   // recheck existence before adding, because it might have been added after read lock release

   if( (poolNormal.find(targetID) != poolNormal.end() ) ||
       (poolLow.find(targetID) != poolLow.end() ) ||
       (poolEmergency.find(targetID) != poolEmergency.end() ) )
   { // found target => we're done
      foundTarget = true;
   }
   else
      addUncheckedUnlocked(targetID, poolType);

   writeLock.unlock(); // U N L O C K write

   return !foundTarget;
}


void TargetCapacityPools::remove(uint16_t targetID)
{
   SafeRWLock lock(&rwlock, SafeRWLock_WRITE);

   if(poolNormal.erase(targetID) )
   {
      // do nothing (just don't return here because of unlock)
   }
   else
   if(poolLow.erase(targetID) )
   {
      // do nothing (just don't return here because of unlock)
   }
   else
      poolEmergency.erase(targetID);

   lock.unlock();
}

void TargetCapacityPools::syncPoolsFromLists(UInt16List& listNormal, UInt16List& listLow,
   UInt16List& listEmergency)
{
   // we fill temporary sets first without lock, then swap with lock (to keep write-lock time short)

   UInt16Set poolNormalNewTmp(listNormal.begin(), listNormal.end() );
   UInt16Set poolLowNewTmp(listLow.begin(), listLow.end() );
   UInt16Set poolEmergencyNewTmp(listEmergency.begin(), listEmergency.end() );


   SafeRWLock lock(&rwlock, SafeRWLock_WRITE);

   poolNormalNewTmp.swap(poolNormal);
   poolLowNewTmp.swap(poolLow);
   poolEmergencyNewTmp.swap(poolEmergency);

   lock.unlock();
}

void TargetCapacityPools::getPoolsAsLists(UInt16List& outListNormal, UInt16List& outListLow,
   UInt16List& outListEmergency)
{
   SafeRWLock lock(&rwlock, SafeRWLock_READ);

   for(UInt16SetIter iter = poolNormal.begin(); iter != poolNormal.end(); iter++)
      outListNormal.push_back(*iter);

   for(UInt16SetIter iter = poolLow.begin(); iter != poolLow.end(); iter++)
      outListLow.push_back(*iter);

   for(UInt16SetIter iter = poolEmergency.begin(); iter != poolEmergency.end(); iter++)
      outListEmergency.push_back(*iter);

   lock.unlock();
}



/**
 * @param preferredTargets may be NULL
 */
void TargetCapacityPools::chooseStorageTargets(unsigned numTargets, UInt16List* preferredTargets,
   UInt16Vector* outTargets)
{
   SafeRWLock lock(&rwlock, SafeRWLock_READ);

   if(!preferredTargets || preferredTargets->empty() )
   { // no preference => use first pool that contains any targets
      UInt16Set* pool;

      if(poolNormal.size() )
         pool = &poolNormal;
      else
      if(poolLow.size() )
         pool = &poolLow;
      else
         pool = &poolEmergency;

      chooseStorageNodesNoPref(*pool, numTargets, outTargets);
   }
   else
   {
      // caller has preferred targets, so we can't say a priori whether nodes will be found or not
      // in a pool. our strategy here is to automatically allow non-preferred nodes before touching
      // the emergency pool.

      // try normal and low pool with preferred targets...

      chooseStorageNodesWithPref(poolNormal, numTargets, preferredTargets, false, outTargets);
      if(!outTargets->empty() )
         goto unlock_and_exit;

      chooseStorageNodesWithPref(poolLow, numTargets, preferredTargets, false, outTargets);
      if(!outTargets->empty() )
         goto unlock_and_exit;

      // allow also non-preferred targets before we use the emergeny pool...

      chooseStorageNodesWithPref(poolNormal, numTargets, preferredTargets, true, outTargets);
      if(!outTargets->empty() )
         goto unlock_and_exit;

      chooseStorageNodesWithPref(poolLow, numTargets, preferredTargets, true, outTargets);
      if(!outTargets->empty() )
         goto unlock_and_exit;

      // still no nodes available => we have to try the emergency pool (first with preferrence, then
      // without preferrence)

      chooseStorageNodesWithPref(poolEmergency, numTargets, preferredTargets, false, outTargets);
      if(!outTargets->empty() )
         goto unlock_and_exit;

      chooseStorageNodesWithPref(poolEmergency, numTargets, preferredTargets, true, outTargets);
   }


unlock_and_exit:

   lock.unlock();
}

/**
 * Alloc storage targets in a round-robin fashion.
 * Disadvantages:
 *  - no support for preferred targets
 *  - requires write lock
 *  - lastTarget is not on a per-pool basis and hence would not be perfectly round-robin when
 *    targets are moved to other pools
 *  - lastTarget is not saved across server restart
 * ...so this should only be used in special scenarios and not as a general replacement for the
 * normal chooseStorageTargets() method.
 */
void TargetCapacityPools::chooseStorageTargetsRoundRobin(unsigned numTargets,
   UInt16Vector* outTargets)
{
   SafeRWLock lock(&rwlock, SafeRWLock_WRITE);

   // no preference settings supported => use first pool that contains any targets
   UInt16Set* pool;

   if(poolNormal.size() )
      pool = &poolNormal;
   else
   if(poolLow.size() )
      pool = &poolLow;
   else
      pool = &poolEmergency;

   chooseStorageNodesNoPrefRoundRobin(*pool, numTargets, outTargets);

   lock.unlock();
}


/**
 * Note: Unlocked (=> caller must hold read lock)
 *
 * @param outTargets might cotain less than numTargets if not enough targets are known
 */
void TargetCapacityPools::chooseStorageNodesNoPref(UInt16Set& activeTargets,
   unsigned numTargets, UInt16Vector* outTargets)
{
   // note: we use the name "activeTargets" for the pool here to keep the code very similar to the
      // nodes chooser in the NodeStore class

   if(activeTargets.empty() )
      return; // there's nothing we can do without any storage targets

   unsigned activeTargetsSize = activeTargets.size();

   if(numTargets > activeTargetsSize)
      numTargets = activeTargetsSize;

   UInt16SetIter iter;
   int partSize = activeTargetsSize / numTargets;

   moveIterToRandomElem<UInt16Set, UInt16SetIter>(activeTargets, iter);

   outTargets->reserve(numTargets);

   // for each target in numTargets
   for(unsigned i=0; i < numTargets; i++)
   {
      int rangeMin = i*partSize;
      int rangeMax = (i==(numTargets-1) ) ? (activeTargetsSize-1) : (rangeMin + partSize - 1);
      // rangeMax decision because numTargets might not devide activeTargetsSize without a remainder

      int next = randGen.getNextInRange(rangeMin, rangeMax);

      /* move iter to the chosen target, add it to outTargets, and skip the remaining targets of
         this part */

      for(int j=rangeMin; j < next; j++)
         moveIterToNextRingElem<UInt16Set, UInt16SetIter>(activeTargets, iter);

      outTargets->push_back(*iter);

      for(int j=next; j < (rangeMax+1); j++)
         moveIterToNextRingElem<UInt16Set, UInt16SetIter>(activeTargets, iter);
   }

}

/**
 * Note: Unlocked (=> caller must hold write lock)
 *
 * @param outTargets might cotain less than numTargets if not enough targets are known
 */
void TargetCapacityPools::chooseStorageNodesNoPrefRoundRobin(UInt16Set& activeTargets,
   unsigned numTargets, UInt16Vector* outTargets)
{
   // note: we use the name "activeTargets" for the pool here to keep the code very similar to the
      // nodes chooser in the NodeStore class

   if(activeTargets.empty() )
      return; // there's nothing we can do without any storage targets

   unsigned activeTargetsSize = activeTargets.size();

   if(numTargets > activeTargetsSize)
      numTargets = activeTargetsSize;

   UInt16SetIter iter = activeTargets.lower_bound(lastRoundRobinTarget);
   if(iter == activeTargets.end() )
      iter = activeTargets.begin();

   outTargets->reserve(numTargets);

   // just add the requested number of targets sequentially from the set
   for(unsigned i=0; i < numTargets; i++)
   {
      moveIterToNextRingElem<UInt16Set, UInt16SetIter>(activeTargets, iter);
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
void TargetCapacityPools::chooseStorageNodesWithPref(UInt16Set& activeTargets,
   unsigned numTargets, UInt16List* preferredTargets, bool allowNonPreferredTargets,
   UInt16Vector* outTargets)
{
   // note: we use the name "activeTargets" for the pool here to keep the code very similar to the
      // nodes chooser in the NodeStore class

   if(activeTargets.empty() )
      return; // there's nothing we can do without any storage targets

   unsigned activeTargetsSize = activeTargets.size();

   // max number of targets is limited by the number of known active targets
   if(numTargets > activeTargetsSize)
      numTargets = activeTargetsSize;

   // Stage 1: add all the preferred targets that are actually available to the outTargets

   // note: we use a separate set for the outTargets here to quickly find out (in stage 2) whether
   // we already added a certain node from the preferred targets (in stage 1)

   UInt16Set outTargetsSet; // (see note above)
   UInt16ListIter preferredIter;
   UInt16SetIter activeTargetsIter; // (will be re-used in stage 2)

   moveIterToRandomElem<UInt16List, UInt16ListIter>(*preferredTargets, preferredIter);

   outTargets->reserve(numTargets);

   // walk over all the preferred targets and add them to outTargets when they are available
   // (note: iterTmp is used to avoid calling preferredTargets->size() )
   for(UInt16ListIter iterTmp = preferredTargets->begin();
       (iterTmp != preferredTargets->end() ) && numTargets;
       iterTmp++)
   {
      activeTargetsIter = activeTargets.find(*preferredIter);

      if(activeTargetsIter != activeTargets.end() )
      { // this preferred node is active => add to outTargets and to outTargetsSet
         outTargets->push_back(*preferredIter);
         outTargetsSet.insert(*preferredIter);

         numTargets--;
      }

      moveIterToNextRingElem<UInt16List, UInt16ListIter>(*preferredTargets, preferredIter);
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
         outTargetsSetIter = outTargetsSet.find(*activeTargetsIter);
         if(outTargetsSetIter == outTargetsSet.end() )
         {
            outTargets->push_back(*activeTargetsIter);
            outTargetsSet.insert(*activeTargetsIter);

            numTargets--;
         }

         moveIterToNextRingElem(activeTargets, activeTargetsIter);
      }
   }

}

/**
 * @return pointer to static string buffer (no free needed)
 */
const char* TargetCapacityPools::poolTypeToStr(TargetCapacityPoolType poolType)
{
   switch(poolType)
   {
      case TargetCapacityPool_NORMAL:
      {
         return "Normal";
      } break;

      case TargetCapacityPool_LOW:
      {
         return "Low";
      } break;

      case TargetCapacityPool_EMERGENCY:
      {
         return "Emergency";
      } break;

      default:
      {
         return "Unknown/Invalid";
      } break;
   }
}
