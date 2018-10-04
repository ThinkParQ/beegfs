#ifndef TARGETSTATESTORE_H_
#define TARGETSTATESTORE_H_

#include <common/nodes/MirrorBuddyGroup.h>
#include <common/nodes/Node.h>
#include <common/nodes/TargetStateInfo.h>
#include <common/threading/RWLockGuard.h>
#include <common/logging/Backtrace.h>


class MirrorBuddyGroupMapper; // forward declaration


class TargetStateStore
{
   public:
      TargetStateStore(NodeType nodeType);

      void addIfNotExists(uint16_t targetID, CombinedTargetState state);
      void removeTarget(uint16_t targetID);

      void syncStatesAndGroups(MirrorBuddyGroupMapper* buddyGroups, const TargetStateMap& states,
            MirrorBuddyGroupMap newGroups, const NumNodeID localNodeID);
      void syncStatesFromLists(const UInt16List& targetIDs, const UInt8List& reachabilityStates,
         const UInt8List& consistencyStates);

      void getStatesAndGroups(const MirrorBuddyGroupMapper* buddyGroups,
         TargetStateMap& states, MirrorBuddyGroupMap& outGroups) const;
      void getStatesAsLists(UInt16List& outTargetIDs, UInt8List& outReachabilityStates,
         UInt8List& outConsistencyStates) const;

      static const char* stateToStr(TargetReachabilityState state);
      static const char* stateToStr(TargetConsistencyState state);
      static const std::string stateToStr(const CombinedTargetState& state);

   protected:
      mutable RWLock rwlock;

      TargetStateInfoMap statesMap;

   private:
      void getStatesAsListsUnlocked(UInt16List& outTargetIDs,
         UInt8List& outReachabilityStates, UInt8List& outConsistencyStates) const;

   public:
      // getters & setters
      bool getStateInfo(uint16_t targetID, TargetStateInfo& outStateInfo) const
      {
         RWLockGuard safeLock(rwlock, SafeRWLock_READ);
         return getStateInfoUnlocked(targetID, outStateInfo);
      }

      bool getState(uint16_t targetID, CombinedTargetState& outState) const
      {
         RWLockGuard safeLock(rwlock, SafeRWLock_READ);
         return getStateUnlocked(targetID, outState);
      }

      void setAllStates(TargetReachabilityState state)
      {
         LOG_DBG(STATES, DEBUG, "Setting all states.", ("New state", stateToStr(state)),
               ("Called from", Backtrace<3>()));
         RWLockGuard safeLock(rwlock, SafeRWLock_WRITE);
         setAllStatesUnlocked(state);
      }

      void setState(uint16_t id, CombinedTargetState state)
      {
         LOG_DBG(STATES, DEBUG, "Setting new state.", id,
               ("New state", stateToStr(state)), ("Called from", Backtrace<3>()));
         RWLockGuard safeLock(rwlock, SafeRWLock_WRITE);
         statesMap[id] = state;
      }

      void setReachabilityState(uint16_t id, TargetReachabilityState state)
      {
         LOG_DBG(STATES, DEBUG, "Setting new reachability state.", id,
               ("New state", stateToStr(state)), ("Called from", Backtrace<3>()));
         RWLockGuard safeLock(rwlock, SafeRWLock_WRITE);
         statesMap[id].reachabilityState = state;
      }

      void setConsistencyState(uint16_t id, TargetConsistencyState state)
      {
         LOG_DBG(STATES, DEBUG, "Setting new consistency state.", id,
               ("New state", stateToStr(state)), ("Called from", Backtrace<3>()));
         RWLockGuard safeLock(rwlock, SafeRWLock_WRITE);
         statesMap[id].consistencyState = state;
      }


   protected:
      NodeType nodeType;

      bool getStateUnlocked(uint16_t targetID, CombinedTargetState& outState) const
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
      bool getStateInfoUnlocked(uint16_t targetID, TargetStateInfo& outStateInfo) const
      {
         TargetStateInfoMapConstIter iter = this->statesMap.find(targetID);
         if (unlikely(iter == statesMap.end() ) )
            return false;

         outStateInfo = iter->second;
         return true;
      }
};

#endif /* TARGETSTATESTORE_H_ */
