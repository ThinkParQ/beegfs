#ifndef MIRRORBUDDYGROUPMAPPER_H_
#define MIRRORBUDDYGROUPMAPPER_H_

#include <common/nodes/NodeCapacityPools.h>
#include <common/nodes/TargetMapper.h>
#include <common/threading/SafeRWLock.h>
#include <common/threading/SafeMutexLock.h>
#include <common/Common.h>
#include <common/nodes/NodeStore.h>

#include <limits.h>
#include <algorithm>


#define MIRRORBUDDYGROUPMAPPER_MAX_GROUPIDS (USHRT_MAX-1) /* -1 for reserved value "0" */


struct MirrorBuddyGroup
{
      uint16_t firstTargetID;
      uint16_t secondTargetID;

      MirrorBuddyGroup()
      {
         this->firstTargetID = 0;
         this->secondTargetID = 0;
      }

      MirrorBuddyGroup(uint16_t firstTargetID, uint16_t secondTargetID)
      {
         this->firstTargetID = firstTargetID;
         this->secondTargetID = secondTargetID;
      }
};


typedef std::list<MirrorBuddyGroup> MirrorBuddyGroupList;
typedef MirrorBuddyGroupList::iterator MirrorBuddyGroupListIter;
typedef MirrorBuddyGroupList::const_iterator MirrorBuddyGroupListCIter;

typedef std::map<uint16_t, MirrorBuddyGroup> MirrorBuddyGroupMap; // keys: MBG-IDs, values: MBGs
typedef MirrorBuddyGroupMap::iterator MirrorBuddyGroupMapIter;
typedef MirrorBuddyGroupMap::const_iterator MirrorBuddyGroupMapCIter;
typedef MirrorBuddyGroupMap::value_type MirrorBuddyGroupMapVal;

enum MirrorBuddyState
{
      BuddyState_UNMAPPED,
      BuddyState_PRIMARY,
      BuddyState_SECONDARY,
};


class MirrorBuddyGroupMapper
{
   friend class TargetStateStore; // for atomic update of state change plus mirror group switch
   friend class MgmtdTargetStateStore; // for atomic update of state change plus mirror group switch

   public:
      MirrorBuddyGroupMapper(TargetMapper* targetMapper);
      MirrorBuddyGroupMapper();

      FhgfsOpsErr mapMirrorBuddyGroup(uint16_t buddyGroupID, uint16_t primaryTarget,
         uint16_t secondaryTarget, NumNodeID localNodeID, bool allowUpdate,
         uint16_t* outNewBuddyGroup);
      bool unmapMirrorBuddyGroup(uint16_t buddyGroupID, NumNodeID localNodeID);

      void syncGroupsFromLists(UInt16List& buddyGroupIDs, MirrorBuddyGroupList& buddyGroups,
         NumNodeID localNodeID);
      void syncGroupsFromLists(UInt16List& buddyGroupIDs, UInt16List& primaryTargets,
         UInt16List& secondaryTargets, NumNodeID localNodeID);

      void getMappingAsLists(UInt16List& outBuddyGroupIDs, MirrorBuddyGroupList& outBuddyGroups);
      void getMappingAsLists(UInt16List& outBuddyGroupIDs, UInt16List& outPrimaryTargets,
         UInt16List& outSecondaryTargets);

      bool loadFromFile();
      bool saveToFile();

      uint16_t getBuddyGroupID(uint16_t targetID);
      uint16_t getBuddyGroupID(uint16_t targetID, bool* outTargetIsPrimary);
      uint16_t getBuddyTargetID(uint16_t targetID, bool* outTargetIsPrimary = NULL);

      MirrorBuddyState getBuddyState(uint16_t targetID);
      MirrorBuddyState getBuddyState(uint16_t targetID, uint16_t buddyGroupID);

      void attachCapacityPools(NodeCapacityPools* capacityPools);
      void attachNodeStore(NodeStore* usableNodes);

   private:
      TargetMapper* targetMapper;
      NodeCapacityPools* capacityPools;
      // this node store is only used for metadata nodes to check for their existence when adding
      // them to a BMG
      NodeStore* usableNodes;

      RWLock rwlock;
      MirrorBuddyGroupMap mirrorBuddyGroups;

      bool mappingsDirty; // true if saved mappings file needs to be updated
      std::string storePath; // set to enable load/save methods (setting is not thread-safe)

      uint16_t localGroupID; // the groupID the local node belongs to; 0 if not part of a group

      void exportToStringMap(StringMap& outExportMap);
      void importFromStringMap(StringMap& importMap);

      uint16_t generateID();

      uint16_t getBuddyGroupIDUnlocked(uint16_t targetID, bool* outTargetIsPrimary);

      void getMappingAsListsUnlocked(UInt16List& outBuddyGroupIDs,
         MirrorBuddyGroupList& outBuddyGroups);
      void getMappingAsListsUnlocked(UInt16List& outBuddyGroupIDs, UInt16List& outPrimaryTargets,
         UInt16List& outSecondaryTargets);

      void switchover(uint16_t buddyGroupID);

   public:
      // getters & setters

      /**
       * @return a group with targets [0,0] if ID not found
       */
      MirrorBuddyGroup getMirrorBuddyGroup(uint16_t mirrorBuddyGroupID)
      {
         MirrorBuddyGroup mbg(0,0);

         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         mbg = getMirrorBuddyGroupUnlocked(mirrorBuddyGroupID);

         safeLock.unlock(); // U N L O C K

         return mbg;
      }

      /**
       * @return 0 if group ID not found
       */
      uint16_t getPrimaryTargetID(uint16_t mirrorBuddyGroupID)
      {
         return getMirrorBuddyGroup(mirrorBuddyGroupID).firstTargetID;
      }

      /**
       * @return 0 if group ID not found
       */
      uint16_t getSecondaryTargetID(uint16_t mirrorBuddyGroupID)
      {
         return getMirrorBuddyGroup(mirrorBuddyGroupID).secondTargetID;
      }

      size_t getSize()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         size_t mirrorBuddyGroupsSize = mirrorBuddyGroups.size();

         safeLock.unlock(); // U N L O C K

         return mirrorBuddyGroupsSize;
      }

      void setStorePath(std::string storePath)
      {
         this->storePath = storePath;
      }

      bool isMapperDirty()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         bool retVal = mappingsDirty;

         safeLock.unlock(); // U N L O C K

         return retVal;
      }

      void getMirrorBuddyGroups(MirrorBuddyGroupMap& outMirrorBuddyGroups)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         outMirrorBuddyGroups = mirrorBuddyGroups;

         safeLock.unlock();
      }

      uint16_t getLocalGroupID()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         uint16_t retVal = getLocalGroupIDUnlocked();

         safeLock.unlock();

         return retVal;
      }

      MirrorBuddyGroup getLocalBuddyGroup()
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         const uint16_t localGroupID = getLocalGroupIDUnlocked();
         const MirrorBuddyGroup localGroup = getMirrorBuddyGroupUnlocked(localGroupID);

         safeLock.unlock();

         return localGroup;
      }

   private:

      /**
       * Unlocked version of getSecondaryTargetID. Caller must hold read lock.
       */
      uint16_t getSecondaryTargetIDUnlocked(uint16_t mirrorBuddyGroupID)
      {
         MirrorBuddyGroupMapCIter iter = mirrorBuddyGroups.find(mirrorBuddyGroupID);
         if (likely(iter != mirrorBuddyGroups.end() ) )
            return iter->second.secondTargetID;
         else
            return 0;
      }

      /**
       * Unlocked version of getPrimaryTargetID. Caller must hold read lock.
       */
      uint16_t getPrimaryTargetIDUnlocked(uint16_t mirrorBuddyGroupID)
      {
         MirrorBuddyGroupMapCIter iter = mirrorBuddyGroups.find(mirrorBuddyGroupID);
         if (likely(iter != mirrorBuddyGroups.end() ) )
            return iter->second.firstTargetID;
         else
            return 0;
      }

      uint16_t getLocalGroupIDUnlocked()
      {
         return localGroupID;
      }

      void setLocalGroupIDUnlocked(uint16_t localGroupID)
      {
         this->localGroupID = localGroupID;
      }

      /**
       * Caller must hold read lock.
       */
      MirrorBuddyGroup getMirrorBuddyGroupUnlocked(uint16_t mirrorBuddyGroupID)
      {
         MirrorBuddyGroupMapCIter iter = mirrorBuddyGroups.find(mirrorBuddyGroupID);
         if (likely(iter != mirrorBuddyGroups.end() ) )
            return iter->second;
         else
            return MirrorBuddyGroup(0, 0);
      }
};

#endif /* MIRRORBUDDYGROUPMAPPER_H_ */
