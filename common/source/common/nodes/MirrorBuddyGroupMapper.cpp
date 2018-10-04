#include <common/app/log/LogContext.h>
#include <common/nodes/MirrorBuddyGroupCreator.h>
#include <common/toolkit/MapTk.h>
#include <common/nodes/TargetStateStore.h>

#include "MirrorBuddyGroupMapper.h"

MirrorBuddyGroupMapper::MirrorBuddyGroupMapper(TargetMapper* targetMapper):
      targetMapper(targetMapper), metaCapacityPools(NULL), storagePools(NULL), usableNodes(NULL),
      localGroupID(0) { }

MirrorBuddyGroupMapper::MirrorBuddyGroupMapper():
     targetMapper(NULL), metaCapacityPools(NULL), storagePools(NULL), usableNodes(NULL),
     localGroupID(0) { }

/**
 * Map two target IDs to a buddyGroupID.
 *
 * Note: re-maps buddyGroupID if it was mapped before.
 *
 * @param buddyGroupID my be 0, which creates a new ID
 * @param primaryTarget
 * @param secondaryTarget
 * @param localNodeID nodeNumID of our local node
 * @param allowUpdate if buddyGroupID is given and group exists - is updating allowed?
 * @param outNewBuddyGroup if new group shall be added its ID will be given here, may be NULL
 *
 * @return FhgfsOpsErr indicating the result
 * - FhgfsOpsErr_SUCCESS successful operation
 * - FhgfsOpsErr_INUSE one of the targets is already in use
 * - FhgfsOpsErr_UNKNOWNTARGET one of the targets does not exist
 * - FhgfsOpsErr_EXISTS the buddy group already exists and allowUpdate is false
 */
FhgfsOpsErr MirrorBuddyGroupMapper::mapMirrorBuddyGroup(uint16_t buddyGroupID,
   uint16_t primaryTargetID, uint16_t secondaryTargetID, NumNodeID localNodeID,
   bool allowUpdate = true, uint16_t* outNewBuddyGroup = NULL)
{
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   // sanity checks
   if ( (buddyGroupID != 0) && (!allowUpdate)
      && (this->mirrorBuddyGroups.find(buddyGroupID) != this->mirrorBuddyGroups.end()) )
   {
      return FhgfsOpsErr_EXISTS;
   }

   // dont allow the same ID for primary and secondary
   if (primaryTargetID == secondaryTargetID)
   {
      return FhgfsOpsErr_INVAL;
   }

   if (targetMapper)
   {
      if (! this->targetMapper->targetExists(primaryTargetID) )
         return FhgfsOpsErr_UNKNOWNTARGET;

      if (! this->targetMapper->targetExists(secondaryTargetID) )
         return FhgfsOpsErr_UNKNOWNTARGET;
   }
   else if (usableNodes)
   {
      // for the metadata buddy group mapper we have no targetMapper to check whether node exists
      // or not, so we use the nodeStore that was attached from the App

      const NodeHandle primaryNode = usableNodes->referenceNode(NumNodeID(primaryTargetID));
      if (!primaryNode)
         return FhgfsOpsErr_UNKNOWNNODE;

      const NodeHandle secondaryNode = usableNodes->referenceNode(NumNodeID(secondaryTargetID));
      if (!secondaryNode)
         return FhgfsOpsErr_UNKNOWNNODE;
   }

   const uint16_t primaryInGroup = getBuddyGroupID(primaryTargetID);
   if ( (primaryInGroup > 0) && (primaryInGroup != buddyGroupID))
      return FhgfsOpsErr_INUSE;

   const uint16_t secondaryInGroup = getBuddyGroupID(secondaryTargetID);
   if ( (secondaryInGroup > 0) && (secondaryInGroup != buddyGroupID))
      return FhgfsOpsErr_INUSE;

   // add/update group
   RWLockGuard safeLock(rwlock, SafeRWLock_WRITE);

   // if buddyGroupID is 0, create a new groupID
   if (buddyGroupID == 0)
      buddyGroupID = generateID();

   if (outNewBuddyGroup)
      *outNewBuddyGroup = buddyGroupID;

   const MirrorBuddyGroup mbg(primaryTargetID, secondaryTargetID);

   mirrorBuddyGroups[buddyGroupID] = mbg;

   if (metaCapacityPools)
      metaCapacityPools->addIfNotExists(buddyGroupID, CapacityPool_LOW);

   if (storagePools)
   {
      // automatically add the buddy group to the pool of the primary target
      const StoragePoolPtr storagePool = storagePools->getPool(primaryTargetID);
      if (storagePool)
      {
         const StoragePoolId storagePoolId = storagePool->getId();
         storagePools->addBuddyGroup(buddyGroupID, storagePoolId);
      }
      else
      {
         LOG(MIRRORING, ERR, "Primary target of buddy group is not in a storage pool",
             buddyGroupID, primaryTargetID);
      }
   }

   // set localGroupID, if neccessary
   if (localNodeID == NumNodeID(primaryTargetID) || localNodeID == NumNodeID(secondaryTargetID) )
      localGroupID = buddyGroupID;

   return retVal;
}

/**
 * @return true if the buddyGroupID was unmapped
 */
bool MirrorBuddyGroupMapper::unmapMirrorBuddyGroup(uint16_t buddyGroupID, NumNodeID localNodeID)
{
   RWLockGuard safeLock(rwlock, SafeRWLock_WRITE);

   if ( localGroupID != 0 )
      if ( (localNodeID.val() == mirrorBuddyGroups[buddyGroupID].firstTargetID)
         || (localNodeID.val() == mirrorBuddyGroups[buddyGroupID].secondTargetID) )
         localGroupID = 0;

   const size_t numErased = mirrorBuddyGroups.erase(buddyGroupID);

   if(numErased)
   {
      if (metaCapacityPools)
         metaCapacityPools->remove(buddyGroupID);

      if (storagePools)
         storagePools->removeBuddyGroup(buddyGroupID);
   }

   return (numErased != 0);
}

/**
 * Applies the mapping from two separate lists (keys and values).
 *
 * NOTE: no sanity checks in here
 */
void MirrorBuddyGroupMapper::syncGroupsFromLists(UInt16List& buddyGroupIDs,
   MirrorBuddyGroupList& buddyGroups, NumNodeID localNodeID)
{
   // note: we pre-create a new map and then swap elements to avoid long write-locking
   MirrorBuddyGroupMap newGroups;
   UInt16ListConstIter keysIter = buddyGroupIDs.begin();
   MirrorBuddyGroupListCIter valuesIter = buddyGroups.begin();
   uint16_t localGroupID = 0;

   for( ; keysIter != buddyGroupIDs.end(); keysIter++, valuesIter++)
   {
      newGroups[*keysIter] = *valuesIter;

      if ( (localNodeID.val() == valuesIter->firstTargetID)
         || (localNodeID.val() == valuesIter->secondTargetID) )
         localGroupID = *keysIter;
   }

   RWLockGuard safeWriteLock(rwlock, SafeRWLock_WRITE);

   mirrorBuddyGroups.swap(newGroups);

   if (localGroupID != 0)
      setLocalGroupIDUnlocked(localGroupID);
}

/**
 * Applies the mapping from three separate lists (convenience wrapper around
 * syncGroupsFromLists(key-list,value-list)).
 *
 * NOTE: no sanity checks in here
 *
 */
void MirrorBuddyGroupMapper::syncGroupsFromLists(UInt16List& buddyGroupIDs,
   UInt16List& primaryTargets, UInt16List& secondaryTargets, NumNodeID localNodeID)
{
   MirrorBuddyGroupList mbgList;

   UInt16ListConstIter primaryIter = primaryTargets.begin();
   UInt16ListConstIter secondaryIter = secondaryTargets.begin();

   for( ; primaryIter != primaryTargets.end(); primaryIter++, secondaryIter++)
   {
      const MirrorBuddyGroup mbg(*primaryIter, *secondaryIter);
      mbgList.push_back(mbg);
   }

   syncGroupsFromLists(buddyGroupIDs, mbgList, localNodeID);
}

/**
 * Returns the mapping in two separate lists (keys and values).
 */
void MirrorBuddyGroupMapper::getMappingAsLists(UInt16List& outBuddyGroupIDs,
   MirrorBuddyGroupList& outBuddyGroups) const
{
   RWLockGuard safeLock(rwlock, SafeRWLock_READ);

   getMappingAsListsUnlocked(outBuddyGroupIDs, outBuddyGroups);
}

/**
 * Returns the mapping in two separate lists (keys and values).
 * NOTE: unlocked, so not thread-safe without a lock in the calling function
 */
void MirrorBuddyGroupMapper::getMappingAsListsUnlocked(UInt16List& outBuddyGroupIDs,
   MirrorBuddyGroupList& outBuddyGroups) const
{
   for ( MirrorBuddyGroupMapCIter iter = mirrorBuddyGroups.begin(); iter != mirrorBuddyGroups.end();
      iter++ )
   {
      outBuddyGroupIDs.push_back(iter->first);
      outBuddyGroups.push_back(iter->second);
   }
}

/**
 * Returns the mapping in three separate lists (buddyGroupID, primaryTargetID, secondaryTargetID).
 */
void MirrorBuddyGroupMapper::getMappingAsLists(UInt16List& outBuddyGroupIDs,
   UInt16List& outPrimaryTargets, UInt16List& outSecondaryTargets) const
{
   RWLockGuard safeLock(rwlock, SafeRWLock_READ);

   getMappingAsListsUnlocked(outBuddyGroupIDs, outPrimaryTargets, outSecondaryTargets);
}

/**
 * Returns the mapping in three separate lists (buddyGroupID, primaryTargetID, secondaryTargetID).
 *
 * Note: Caller must hold readlock.
 */
void MirrorBuddyGroupMapper::getMappingAsListsUnlocked(UInt16List& outBuddyGroupIDs,
   UInt16List& outPrimaryTargets, UInt16List& outSecondaryTargets) const
{
   for ( MirrorBuddyGroupMapCIter iter = mirrorBuddyGroups.begin(); iter != mirrorBuddyGroups.end();
      iter++ )
   {
      MirrorBuddyGroup mbg = iter->second;
      outBuddyGroupIDs.push_back(iter->first);
      outPrimaryTargets.push_back(mbg.firstTargetID);
      outSecondaryTargets.push_back(mbg.secondTargetID);
   }
}

/*
 * generate a free buddy group ID.
 *
 * NOTE: unlocked, so not thread-safe without a lock in the calling function
 */
uint16_t MirrorBuddyGroupMapper::generateID() const
{
   typedef MirrorBuddyGroupMap::value_type Group;
   const auto hasGap = [] (const Group& a, const Group& b) {
      return a.first + 1 < b.first;
   };

   if (mirrorBuddyGroups.empty())
      return 1;

   if (mirrorBuddyGroups.rbegin()->first
         < std::numeric_limits<MirrorBuddyGroupMap::key_type>::max())
      return mirrorBuddyGroups.rbegin()->first + 1;

   if (mirrorBuddyGroups.begin()->first > 1)
      return 1;

   const auto gap = std::adjacent_find( mirrorBuddyGroups.begin(), mirrorBuddyGroups.end(), hasGap);
   if (gap != mirrorBuddyGroups.end())
      return gap->first + 1;

   return 0;
}

/*
 * @param targetID the targetID to look for
 *
 * @return the ID of the buddy group this target is mapped to, 0 if unmapped
 */
uint16_t MirrorBuddyGroupMapper::getBuddyGroupID(uint16_t targetID) const
{
   RWLockGuard safeLock(rwlock, SafeRWLock_READ);

   for ( MirrorBuddyGroupMapCIter iter = mirrorBuddyGroups.begin(); iter != mirrorBuddyGroups.end();
      iter++ )
   {
      const MirrorBuddyGroup mbg = iter->second;
      if ( (mbg.firstTargetID == targetID) || (mbg.secondTargetID == targetID) )
      {
         return iter->first;
      }
   }

   // not found
   return 0;
}

/*
 * @param targetID the targetID to look for
 * @param outTargetIsPrimary true if the given targetID is the primary target, false otherwise
 *
 * @return the ID of the buddy group this target is mapped to, 0 if unmapped
 */
uint16_t MirrorBuddyGroupMapper::getBuddyGroupID(uint16_t targetID, bool* outTargetIsPrimary) const
{
   RWLockGuard safeLock(rwlock, SafeRWLock_READ);

   return getBuddyGroupIDUnlocked(targetID, outTargetIsPrimary);
}

/**
 * Unlocked version of getBuddyGroupID(uint16_t, bool*). Caller must hold read lock.
 */
uint16_t MirrorBuddyGroupMapper::getBuddyGroupIDUnlocked(uint16_t targetID,
   bool* outTargetIsPrimary) const
{
   for ( MirrorBuddyGroupMapCIter iter = mirrorBuddyGroups.begin(); iter != mirrorBuddyGroups.end();
      iter++ )
   {
      const MirrorBuddyGroup mbg = iter->second;
      if (mbg.firstTargetID == targetID )
      {
         *outTargetIsPrimary = true;
         return iter->first;
      }
      else if (mbg.secondTargetID == targetID)
      {
         *outTargetIsPrimary = false;
         return iter->first;
      }
   }

   // not found
   *outTargetIsPrimary = false;
   return 0;
}

/*
 * @param targetID the targetID to get the buddy for
 * @param outTargetIsPrimary true if the given target is a primary target (may be NULL if not
 * interested in)
 *
 * @return the other target in this buddy group, 0 if unmapped
 */
uint16_t MirrorBuddyGroupMapper::getBuddyTargetID(uint16_t targetID,
                                                  bool* outTargetIsPrimary) const
{
   RWLockGuard safeLock(rwlock, SafeRWLock_READ);

   for ( MirrorBuddyGroupMapCIter iter = mirrorBuddyGroups.begin(); iter != mirrorBuddyGroups.end();
      iter++ )
   {
      const MirrorBuddyGroup mbg = iter->second;
      if (mbg.firstTargetID == targetID)
      {
         if (outTargetIsPrimary)
            *outTargetIsPrimary = true;

         return mbg.secondTargetID;
      }
      else if (mbg.secondTargetID == targetID)
      {
         if (outTargetIsPrimary)
            *outTargetIsPrimary = false;

         return mbg.firstTargetID;
      }
   }

   // end of loop reached without finding anything
   if (outTargetIsPrimary)
      *outTargetIsPrimary = false;

   return 0;
}

/*
 * @param targetID the target ID to get the property for.
 *
 * @return MirrorBuddyState value indicating whether the target is primary, secondary or unmapped.
 */
MirrorBuddyState MirrorBuddyGroupMapper::getBuddyState(uint16_t targetID) const
{
   RWLockGuard safeLock(rwlock, SafeRWLock_READ);

   for(MirrorBuddyGroupMapCIter iter = mirrorBuddyGroups.begin(); iter != mirrorBuddyGroups.end();
       ++iter)
   {
      const MirrorBuddyGroup& mbg = iter->second;
      if (mbg.firstTargetID == targetID)
      {
         return BuddyState_PRIMARY;
      }
      else if (mbg.secondTargetID == targetID)
      {
         return BuddyState_SECONDARY;
      }
   }

   // end of loop reached without finding anything
   return BuddyState_UNMAPPED;
}

/**
 * Find out whether a target is primary or secondary of a given buddy group. Since only one buddy
 * group is checked, this is faster than getBuddyState(uint16_t targetID).
 *
 * @param targetID the target ID to get the property for.
 * @param buddyGroupID the ID of the buddy group to which the target belongs.
 * @return MirrorBuddyState value indicating whether the target is primary or secondary;
 *         BuddyState_UNMAPPED if target does not belong to this group or group does not exist.
 */
MirrorBuddyState MirrorBuddyGroupMapper::getBuddyState(uint16_t targetID,
                                                       uint16_t buddyGroupID) const
{
   RWLockGuard safeLock(rwlock, SafeRWLock_READ);

   MirrorBuddyGroupMapCIter mbgIter = mirrorBuddyGroups.find(buddyGroupID);

   if (mbgIter != mirrorBuddyGroups.end() )
   {
      const MirrorBuddyGroup& mbg = mbgIter->second;

      if (targetID == mbg.firstTargetID)
         return BuddyState_PRIMARY;
      else if (targetID == mbg.secondTargetID)
         return BuddyState_SECONDARY;
   }

   // not found
   return BuddyState_UNMAPPED;
}

/**
 * buddy groups will automatically be added/removed from attached capacity pools when they are
 * added/removed from this store.
 */
void MirrorBuddyGroupMapper::attachMetaCapacityPools(NodeCapacityPools* capacityPools)
{
   RWLockGuard safeLock(rwlock, SafeRWLock_WRITE);

   this->metaCapacityPools = capacityPools;
}

/**
 * node store is only used for metadata nodes to check for their existence when adding them to a
 * BMG
 */
void MirrorBuddyGroupMapper::attachNodeStore(NodeStore* usableNodes)
{
   RWLockGuard safeLock(rwlock, SafeRWLock_WRITE);

   this->usableNodes = usableNodes;
}

/**
* Buddy groups will automatically be added to the default storage pool when they are added to this
* store.
*/
void MirrorBuddyGroupMapper::attachStoragePoolStore(StoragePoolStore* storagePools)
{
   RWLockGuard const _(rwlock, SafeRWLock_WRITE);

   this->storagePools = storagePools;
}
