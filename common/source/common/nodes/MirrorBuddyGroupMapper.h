#ifndef MIRRORBUDDYGROUPMAPPER_H_
#define MIRRORBUDDYGROUPMAPPER_H_

#include <common/nodes/NodeCapacityPools.h>
#include <common/nodes/TargetMapper.h>
#include <common/threading/SafeRWLock.h>
#include <common/Common.h>
#include <common/nodes/NodeStore.h>
#include <common/nodes/MirrorBuddyGroup.h>

#include <limits.h>
#include <algorithm>


#define MIRRORBUDDYGROUPMAPPER_MAX_GROUPIDS (USHRT_MAX-1) /* -1 for reserved value "0" */


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

      FhgfsOpsErr mapMirrorBuddyGroup(uint16_t buddyGroupID, uint16_t primaryTargetID,
         uint16_t secondaryTargetID, NumNodeID localNodeID, bool allowUpdate,
         uint16_t* outNewBuddyGroup);
      bool unmapMirrorBuddyGroup(uint16_t buddyGroupID, NumNodeID localNodeID);

      void syncGroupsFromLists(UInt16List& buddyGroupIDs, MirrorBuddyGroupList& buddyGroups,
         NumNodeID localNodeID);
      void syncGroupsFromLists(UInt16List& buddyGroupIDs, UInt16List& primaryTargets,
         UInt16List& secondaryTargets, NumNodeID localNodeID);

      void getMappingAsLists(UInt16List& outBuddyGroupIDs,
                             MirrorBuddyGroupList& outBuddyGroups) const;
      void getMappingAsLists(UInt16List& outBuddyGroupIDs, UInt16List& outPrimaryTargets,
                             UInt16List& outSecondaryTargets) const;

      uint16_t getBuddyGroupID(uint16_t targetID) const;
      uint16_t getBuddyGroupID(uint16_t targetID, bool* outTargetIsPrimary) const;
      uint16_t getBuddyTargetID(uint16_t targetID, bool* outTargetIsPrimary = NULL) const;

      MirrorBuddyState getBuddyState(uint16_t targetID) const;
      MirrorBuddyState getBuddyState(uint16_t targetID, uint16_t buddyGroupID) const;

      void attachMetaCapacityPools(NodeCapacityPools* capacityPools);

      void attachStoragePoolStore(StoragePoolStore* storagePools);
      void attachNodeStore(NodeStore* usableNodes);

   protected:
      TargetMapper* targetMapper;
      NodeCapacityPools* metaCapacityPools;

      StoragePoolStore* storagePools;
      // this node store is only used for metadata nodes to check for their existence when adding
      // them to a BMG
      NodeStore* usableNodes;

      mutable RWLock rwlock;
      MirrorBuddyGroupMap mirrorBuddyGroups;

      uint16_t localGroupID; // the groupID the local node belongs to; 0 if not part of a group

      uint16_t generateID() const;

      uint16_t getBuddyGroupIDUnlocked(uint16_t targetID, bool* outTargetIsPrimary) const;

      void getMappingAsListsUnlocked(UInt16List& outBuddyGroupIDs,
         MirrorBuddyGroupList& outBuddyGroups) const;
      void getMappingAsListsUnlocked(UInt16List& outBuddyGroupIDs, UInt16List& outPrimaryTargets,
         UInt16List& outSecondaryTargets) const;

   public:
      // getters & setters

      /**
       * @return a group with targets [0,0] if ID not found
       */
      MirrorBuddyGroup getMirrorBuddyGroup(uint16_t mirrorBuddyGroupID) const
      {
         RWLockGuard safeLock(rwlock, SafeRWLock_READ); // L O C K

         return getMirrorBuddyGroupUnlocked(mirrorBuddyGroupID);
      }

      /**
       * @return 0 if group ID not found
       */
      uint16_t getPrimaryTargetID(uint16_t mirrorBuddyGroupID) const
      {
         return getMirrorBuddyGroup(mirrorBuddyGroupID).firstTargetID;
      }

      /**
       * @return 0 if group ID not found
       */
      uint16_t getSecondaryTargetID(uint16_t mirrorBuddyGroupID) const
      {
         return getMirrorBuddyGroup(mirrorBuddyGroupID).secondTargetID;
      }

      size_t getSize() const
      {
         RWLockGuard safeLock(rwlock, SafeRWLock_READ);

         return mirrorBuddyGroups.size();
      }

      void getMirrorBuddyGroups(MirrorBuddyGroupMap& outMirrorBuddyGroups) const
      {
         RWLockGuard safeLock(rwlock, SafeRWLock_READ);

         outMirrorBuddyGroups = mirrorBuddyGroups;
      }

      uint16_t getLocalGroupID() const
      {
         RWLockGuard safeLock(rwlock, SafeRWLock_READ);

         return getLocalGroupIDUnlocked();
      }

      MirrorBuddyGroup getLocalBuddyGroup() const
      {
         RWLockGuard safeLock(rwlock, SafeRWLock_READ);

         const uint16_t localGroupID = getLocalGroupIDUnlocked();
         return getMirrorBuddyGroupUnlocked(localGroupID);
      }

      MirrorBuddyGroupMap getMapping() const
      {
         RWLockGuard safeLock(rwlock, SafeRWLock_READ);
         return mirrorBuddyGroups;
      }

   private:

      /**
       * Unlocked version of getSecondaryTargetID. Caller must hold read lock.
       */
      uint16_t getSecondaryTargetIDUnlocked(uint16_t mirrorBuddyGroupID) const
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
      uint16_t getPrimaryTargetIDUnlocked(uint16_t mirrorBuddyGroupID) const
      {
         MirrorBuddyGroupMapCIter iter = mirrorBuddyGroups.find(mirrorBuddyGroupID);
         if (likely(iter != mirrorBuddyGroups.end() ) )
            return iter->second.firstTargetID;
         else
            return 0;
      }

      uint16_t getLocalGroupIDUnlocked() const
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
      MirrorBuddyGroup getMirrorBuddyGroupUnlocked(uint16_t mirrorBuddyGroupID) const
      {
         MirrorBuddyGroupMapCIter iter = mirrorBuddyGroups.find(mirrorBuddyGroupID);
         if (likely(iter != mirrorBuddyGroups.end() ) )
            return iter->second;
         else
            return MirrorBuddyGroup(0, 0);
      }
};

#endif /* MIRRORBUDDYGROUPMAPPER_H_ */
