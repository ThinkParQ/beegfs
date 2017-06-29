#ifndef TARGETSTATESTORE_H_
#define TARGETSTATESTORE_H_

#include <common/nodes/TargetStateInfo.h>
#include <common/threading/SafeRWLock.h>


class MirrorBuddyGroupMapper; // forward declaration


class TargetStateStore
{
   public:
      TargetStateStore();

      bool addOrUpdate(uint16_t targetID, CombinedTargetState state);
      void addIfNotExists(uint16_t targetID, CombinedTargetState state);
      void removeTarget(uint16_t targetID);

      void syncStatesAndGroupsFromLists(MirrorBuddyGroupMapper* buddyGroups,
         const UInt16List& targetIDs, const UInt8List& reachabilityStates,
         const UInt8List& consistencyStates, const UInt16List& buddyGroupIDs,
         const UInt16List& primaryTargets, const UInt16List& secondaryTargets,
         const NumNodeID localNodeID);
      void syncStatesFromLists(const UInt16List& targetIDs, const UInt8List& reachabilityStates,
         const UInt8List& consistencyStates);

      void getStatesAndGroupsAsLists(MirrorBuddyGroupMapper* buddyGroups, UInt16List& outTargetIDs,
         UInt8List& outReachabilityStates, UInt8List& outConsistencyStates,
         UInt16List& outBuddyGroupIDs, UInt16List& outPrimaryTargets,
         UInt16List& outSecondaryTargets);
      void getStatesAsLists(UInt16List& outTargetIDs, UInt8List& outReachabilityStates,
         UInt8List& outConsistencyStates);

      static const char* stateToStr(TargetReachabilityState state);
      static const char* stateToStr(TargetConsistencyState state);
      static const std::string stateToStr(const CombinedTargetState& state);

   protected:
      RWLock rwlock;

      TargetStateInfoMap statesMap;

   private:
      void getStatesAsListsUnlocked(UInt16List& outTargetIDs,
         UInt8List& outReachabilityStates, UInt8List& outConsistencyStates);

   public:
      // getters & setters
      bool getStateInfo(uint16_t targetID, TargetStateInfo& outStateInfo)
      {
         bool res;

         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         res = getStateInfoUnlocked(targetID, outStateInfo);

         safeLock.unlock(); // U N L O C K

         return res;
      }

      bool getState(uint16_t targetID, CombinedTargetState& outState)
      {
         bool res;

         SafeRWLock safeLock(&rwlock, SafeRWLock_READ); // L O C K

         res = getStateUnlocked(targetID, outState);

         safeLock.unlock(); // U N L O C K

         return res;
      }

      void setAllStates(TargetReachabilityState state)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

         setAllStatesUnlocked(state);

         safeLock.unlock(); // U N L O C K
      }

      void setState(uint16_t id, CombinedTargetState state)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE); // L O C K

         statesMap[id] = state;

         safeLock.unlock(); // U N L O C K
      }

      void setReachabilityState(uint16_t id, TargetReachabilityState state)
      {
         SafeRWLock safeLock(&rwlock, SafeRWLock_WRITE);

         statesMap[id].reachabilityState = state;

         safeLock.unlock();
      }


   protected:

      bool getStateUnlocked(uint16_t targetID, CombinedTargetState& outState)
      {
         TargetStateInfoMapConstIter iter = this->statesMap.find(targetID);
         if (unlikely(iter == statesMap.end() ) )
            return false;

         outState = iter->second;
         return true;
      }

      void setAllStatesUnlocked(TargetReachabilityState state)
      {
         for (TargetStateInfoMapIter iter = this->statesMap.begin();
            iter != this->statesMap.end(); ++iter)
         {
            TargetStateInfo& currentState = iter->second;
            if (currentState.reachabilityState != state)
            {
               currentState.reachabilityState = state;
               currentState.lastChangedTime.setToNow();
            }
         }
      }


   private:
      bool getStateInfoUnlocked(uint16_t targetID, TargetStateInfo& outStateInfo)
      {
         TargetStateInfoMapConstIter iter = this->statesMap.find(targetID);
         if (unlikely(iter == statesMap.end() ) )
            return false;

         outStateInfo = iter->second;
         return true;
      }
};

#endif /* TARGETSTATESTORE_H_ */
