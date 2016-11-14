#include <common/app/log/LogContext.h>
#include <common/nodes/MirrorBuddyGroupCreator.h>
#include <common/toolkit/MapTk.h>
#include <common/toolkit/Random.h>
#include <common/nodes/TargetStateStore.h>

#include "MirrorBuddyGroupMapper.h"

MirrorBuddyGroupMapper::MirrorBuddyGroupMapper(TargetMapper* targetMapper)
   : targetMapper(targetMapper), capacityPools(NULL), mappingsDirty(false), localGroupID(0)
{ }

MirrorBuddyGroupMapper::MirrorBuddyGroupMapper()
   : targetMapper(NULL), capacityPools(NULL), mappingsDirty(false), localGroupID(0)
{ }

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

   if (targetMapper)
   {
      if (! this->targetMapper->targetExists(primaryTargetID) )
         return FhgfsOpsErr_UNKNOWNTARGET;

      if (! this->targetMapper->targetExists(secondaryTargetID) )
         return FhgfsOpsErr_UNKNOWNTARGET;
   }

   uint16_t primaryInGroup = getBuddyGroupID(primaryTargetID);
   if ( (primaryInGroup > 0) && (primaryInGroup != buddyGroupID))
      return FhgfsOpsErr_INUSE;

   uint16_t secondaryInGroup = getBuddyGroupID(secondaryTargetID);
   if ( (secondaryInGroup > 0) && (secondaryInGroup != buddyGroupID))
      return FhgfsOpsErr_INUSE;

   // add/update group
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   // if buddyGroupID is 0, create a new groupID
   if (buddyGroupID == 0)
      buddyGroupID = generateID();

   if (outNewBuddyGroup)
      *outNewBuddyGroup = buddyGroupID;

   MirrorBuddyGroup mbg(primaryTargetID, secondaryTargetID);

   mirrorBuddyGroups[buddyGroupID] = mbg;

   if (capacityPools)
      capacityPools->addIfNotExists(buddyGroupID, CapacityPool_LOW);

   // set localGroupID, if neccessary
   if (localNodeID == NumNodeID(primaryTargetID) || localNodeID == NumNodeID(secondaryTargetID) )
      localGroupID = buddyGroupID;

   safeLock.unlock(); // U N L O C K

   mappingsDirty = true;

   return retVal;
}

/**
 * @return true if the buddyGroupID was unmapped
 */
bool MirrorBuddyGroupMapper::unmapMirrorBuddyGroup(uint16_t buddyGroupID, NumNodeID localNodeID)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   if ( localGroupID != 0 )
      if ( (localNodeID.val() == mirrorBuddyGroups[buddyGroupID].firstTargetID)
         || (localNodeID.val() == mirrorBuddyGroups[buddyGroupID].secondTargetID) )
         localGroupID = 0;

   size_t numErased = mirrorBuddyGroups.erase(buddyGroupID);

   if(numErased)
   {
      mappingsDirty = true;
      if (capacityPools)
         capacityPools->remove(buddyGroupID);
   }

   safeLock.unlock(); // U N L O C K

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

   SafeRWLock safeWriteLock(&rwlock, SafeRWLock_WRITE); // L O C K (write)

   mirrorBuddyGroups.swap(newGroups);

   mappingsDirty = true;

   if (localGroupID != 0)
      setLocalGroupIDUnlocked(localGroupID);

   safeWriteLock.unlock(); // U N L O C K (write)
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
      MirrorBuddyGroup mbg(*primaryIter, *secondaryIter);
      mbgList.push_back(mbg);
   }

   syncGroupsFromLists(buddyGroupIDs, mbgList, localNodeID);
}

/**
 * Returns the mapping in two separate lists (keys and values).
 */
void MirrorBuddyGroupMapper::getMappingAsLists(UInt16List& outBuddyGroupIDs,
   MirrorBuddyGroupList& outBuddyGroups)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   getMappingAsListsUnlocked(outBuddyGroupIDs, outBuddyGroups);

   safeLock.unlock(); // U N L O C K
}

/**
 * Returns the mapping in two separate lists (keys and values).
 * NOTE: unlocked, so not thread-safe without a lock in the calling function
 */
void MirrorBuddyGroupMapper::getMappingAsListsUnlocked(UInt16List& outBuddyGroupIDs,
   MirrorBuddyGroupList& outBuddyGroups)
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
   UInt16List& outPrimaryTargets, UInt16List& outSecondaryTargets)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   getMappingAsListsUnlocked(outBuddyGroupIDs, outPrimaryTargets, outSecondaryTargets);

   safeLock.unlock(); // U N L O C K
}

/**
 * Returns the mapping in three separate lists (buddyGroupID, primaryTargetID, secondaryTargetID).
 *
 * Note: Caller must hold readlock.
 */
void MirrorBuddyGroupMapper::getMappingAsListsUnlocked(UInt16List& outBuddyGroupIDs,
   UInt16List& outPrimaryTargets, UInt16List& outSecondaryTargets)
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

/**
 * Note: setStorePath must be called before using this.
 */
bool MirrorBuddyGroupMapper::loadFromFile()
{
   bool loaded = false;

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   if ( !storePath.length() )
      goto unlock_and_exit;

   try
   {
      StringMap newGroupsStrMap;

      // load from file
      MapTk::loadStringMapFromFile(storePath.c_str(), &newGroupsStrMap);

      // apply loaded targets
      mirrorBuddyGroups.clear();
      importFromStringMap(newGroupsStrMap);

      mappingsDirty = true;

      loaded = true;
   }
   catch (InvalidConfigException& e)
   {
      LOG_DEBUG(__func__, Log_DEBUG, "Unable to open mirror buddy groups mappings file: "
         + storePath + ". " + "SysErr: " + System::getErrString());
   }

   unlock_and_exit:

   safeLock.unlock(); // U N L O C K

   return loaded;
}

/**
 * Note: setStorePath must be called before using this.
 */
bool MirrorBuddyGroupMapper::saveToFile()
{
   bool saved = false;

   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   if ( storePath.empty() )
      goto unlock_and_exit;

   try
   {
      StringMap groupsStrMap;

      exportToStringMap(groupsStrMap);

      MapTk::saveStringMapToFile(storePath.c_str(), &groupsStrMap);

      mappingsDirty = false;

      saved = true;
   }
   catch (InvalidConfigException& e)
   {
      LogContext(__func__).logErr(
         "Unable to save mirror buddy groups mappings file: " + storePath + ". " + "SysErr: "
            + System::getErrString());
   }

   unlock_and_exit:

   safeLock.unlock(); // U N L O C K

   return saved;
}

/**
 * Note: unlocked, caller must hold lock.
 */
void MirrorBuddyGroupMapper::exportToStringMap(StringMap& outExportMap)
{
   for ( MirrorBuddyGroupMapIter iter = mirrorBuddyGroups.begin(); iter != mirrorBuddyGroups.end();
      iter++ )
   {
      MirrorBuddyGroup mbg = iter->second;

      std::string mbgStr = StringTk::uint16ToHexStr(mbg.firstTargetID) + ","
         + StringTk::uint16ToHexStr(mbg.secondTargetID);

      outExportMap[StringTk::uintToHexStr(iter->first)] = mbgStr;
   }
}

/**
 * Note: unlocked, caller must hold lock.
 * Note: the internal map is not cleared in this method.
 */
void MirrorBuddyGroupMapper::importFromStringMap(StringMap& importMap)
{
   for(StringMapIter iter = importMap.begin(); iter != importMap.end(); iter++)
   {
      uint16_t groupID = StringTk::strHexToUInt(iter->first);
      std::string mbgStr = iter->second;
      StringVector mbgStrVec;

      StringTk::explode(mbgStr, ',', &mbgStrVec);

      uint16_t primaryTargetID = 0;
      uint16_t secondaryTargetID = 0;

      if (mbgStrVec.size() > 1)
      {
         primaryTargetID = StringTk::strHexToUInt(mbgStrVec[0]);
         secondaryTargetID = StringTk::strHexToUInt(mbgStrVec[1]);
      }
      else
      {
         LogContext(__func__).logErr("Corrupt importMap supplied");
      }

      mirrorBuddyGroups[groupID] = MirrorBuddyGroup(primaryTargetID, secondaryTargetID);
   }
}

/*
 * generate a free buddy group ID.
 *
 * NOTE: unlocked, so not thread-safe without a lock in the calling function
 */
uint16_t MirrorBuddyGroupMapper::generateID()
{
   UInt16List groupIDs;
   MirrorBuddyGroupList groupList;

   getMappingAsListsUnlocked(groupIDs, groupList);

   return MirrorBuddyGroupCreator::generateID(&groupIDs);
}

/*
 * @param targetID the targetID to look for
 *
 * @return the ID of the buddy group this target is mapped to, 0 if unmapped
 */
uint16_t MirrorBuddyGroupMapper::getBuddyGroupID(uint16_t targetID)
{
   uint16_t buddyGroupID = 0;

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   for ( MirrorBuddyGroupMapIter iter = mirrorBuddyGroups.begin(); iter != mirrorBuddyGroups.end();
      iter++ )
   {
      MirrorBuddyGroup mbg = iter->second;
      if ( (mbg.firstTargetID == targetID) || (mbg.secondTargetID == targetID) )
      {
         buddyGroupID = iter->first;
         break;
      }
   }

   safeLock.unlock();

   return buddyGroupID;
}

/*
 * @param targetID the targetID to look for
 * @param outTargetIsPrimary true if the given targetID is the primary target, false otherwise
 *
 * @return the ID of the buddy group this target is mapped to, 0 if unmapped
 */
uint16_t MirrorBuddyGroupMapper::getBuddyGroupID(uint16_t targetID, bool* outTargetIsPrimary)
{
   uint16_t buddyGroupID;

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   buddyGroupID = getBuddyGroupIDUnlocked(targetID, outTargetIsPrimary);

   safeLock.unlock(); // U N L O C K

   return buddyGroupID;
}

/**
 * Unlocked version of getBuddyGroupID(uint16_t, bool*). Caller must hold read lock.
 */
uint16_t MirrorBuddyGroupMapper::getBuddyGroupIDUnlocked(uint16_t targetID,
   bool* outTargetIsPrimary)
{
   uint16_t buddyGroupID = 0;
   *outTargetIsPrimary = false;

   for ( MirrorBuddyGroupMapIter iter = mirrorBuddyGroups.begin(); iter != mirrorBuddyGroups.end();
      iter++ )
   {
      MirrorBuddyGroup mbg = iter->second;
      if (mbg.firstTargetID == targetID )
      {
         buddyGroupID = iter->first;
         *outTargetIsPrimary = true;
         break;
      }
      else
      if (mbg.secondTargetID == targetID)
      {
         buddyGroupID = iter->first;
         break;
      }
   }

   return buddyGroupID;
}

/*
 * @param targetID the targetID to get the buddy for
 * @param outTargetIsPrimary true if the given target is a primary target (may be NULL if not
 * interested in)
 *
 * @return the other target in this buddy group, 0 if unmapped
 */
uint16_t MirrorBuddyGroupMapper::getBuddyTargetID(uint16_t targetID, bool* outTargetIsPrimary)
{
   uint16_t buddyTargetID = 0;

   if (outTargetIsPrimary)
      *outTargetIsPrimary = false; // false per default

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   for ( MirrorBuddyGroupMapIter iter = mirrorBuddyGroups.begin(); iter != mirrorBuddyGroups.end();
      iter++ )
   {
      MirrorBuddyGroup mbg = iter->second;
      if (mbg.firstTargetID == targetID)
      {
         buddyTargetID = mbg.secondTargetID;
         if (outTargetIsPrimary)
            *outTargetIsPrimary = true;
         break;
      }
      else
      if (mbg.secondTargetID == targetID)
      {
         buddyTargetID = mbg.firstTargetID;
         break;
      }
   }

   safeLock.unlock();

   return buddyTargetID;
}

/*
 * @param targetID the target ID to get the property for.
 *
 * @return MirrorBuddyState value indicating whether the target is primary, secondary or unmapped.
 */
MirrorBuddyState MirrorBuddyGroupMapper::getBuddyState(uint16_t targetID)
{
   MirrorBuddyState result = BuddyState_UNMAPPED;

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   for(MirrorBuddyGroupMapCIter iter = mirrorBuddyGroups.begin(); iter != mirrorBuddyGroups.end();
       ++iter)
   {
      const MirrorBuddyGroup& mbg = iter->second;
      if (mbg.firstTargetID == targetID)
      {
         result = BuddyState_PRIMARY;
         break;
      }
      else
      if (mbg.secondTargetID == targetID)
      {
         result = BuddyState_SECONDARY;
         break;
      }
   }

   safeLock.unlock(); // U N L O C K

   return result;
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
MirrorBuddyState MirrorBuddyGroupMapper::getBuddyState(uint16_t targetID, uint16_t buddyGroupID)
{
   MirrorBuddyState result = BuddyState_UNMAPPED;

   SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

   MirrorBuddyGroupMapCIter mbgIter = mirrorBuddyGroups.find(buddyGroupID);

   if (mbgIter != mirrorBuddyGroups.end() )
   {
      const MirrorBuddyGroup& mbg = mbgIter->second;

      if (targetID == mbg.firstTargetID)
         result = BuddyState_PRIMARY;
      else
      if (targetID == mbg.secondTargetID)
         result = BuddyState_SECONDARY;
   }

   safeLock.unlock(); // U N L O C K

   return result;
}

/**
 * buddy groups will automatically be added/removed from attached capacity pools when they are
 * added/removed from this store.
 */
void MirrorBuddyGroupMapper::attachCapacityPools(NodeCapacityPools* capacityPools)
{
   SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

   this->capacityPools = capacityPools;

   safeLock.unlock(); // U N L O C K
}

/**
 * Switch buddy buddy group's primary and secondary targets.
 *
 * Note: Unlocked, caller must hold writelock.
 * Note: Private, but intended to be called by friend class MgmtdTargetStateStore.
 */
void MirrorBuddyGroupMapper::switchover(uint16_t buddyGroupID)
{
   const char* logContext = "Primary/secondary switch";

   MirrorBuddyGroupMapIter iter = mirrorBuddyGroups.find(buddyGroupID);

   if (unlikely(iter == mirrorBuddyGroups.end() ) )
   {
      LogContext(logContext).log(Log_ERR, "Unknown mirror group ID: "
         + StringTk::uintToStr(buddyGroupID) );

      return;
   }

   MirrorBuddyGroup& mbg = iter->second;

   LogContext(logContext).log(Log_WARNING, "Switching primary and secondary target. "
         "Mirror group ID: "+ StringTk::uintToStr(iter->first) );

   std::swap(mbg.firstTargetID, mbg.secondTargetID);
   mappingsDirty = true;
}

