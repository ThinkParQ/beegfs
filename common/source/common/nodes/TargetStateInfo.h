#ifndef TARGETSTATEINFO_H_
#define TARGETSTATEINFO_H_

#include <common/Common.h>
#include <common/toolkit/Time.h>
#include <common/toolkit/serialization/Serialization.h>

// Make sure to keep this in sync with corresponding enums in client.
enum TargetReachabilityState
{
   TargetReachabilityState_ONLINE,
   TargetReachabilityState_POFFLINE, // Probably offline.
   TargetReachabilityState_OFFLINE
};

enum TargetConsistencyState
{
   TargetConsistencyState_GOOD,
   TargetConsistencyState_NEEDS_RESYNC,
   TargetConsistencyState_BAD
};

template<>
struct SerializeAs<TargetConsistencyState>
{
   typedef uint8_t type;
};

template<>
struct SerializeAs<TargetReachabilityState>
{
   typedef uint8_t type;
};

typedef std::vector<TargetConsistencyState> TargetConsistencyStateVec;

/**
 * A combined target state is a pair of TargetReachabilityState and TargetConsistencyState.
 */
struct CombinedTargetState
{
   TargetReachabilityState reachabilityState;
   TargetConsistencyState consistencyState;

   CombinedTargetState()
      : reachabilityState(TargetReachabilityState_OFFLINE),
         consistencyState(TargetConsistencyState_GOOD)
   { }

   CombinedTargetState(TargetReachabilityState reachabilityState,
         TargetConsistencyState consistencyState)
      : reachabilityState(reachabilityState),
         consistencyState(consistencyState)
   { }

   bool operator!=(const CombinedTargetState& other) const
   {
      return ( (reachabilityState != other.reachabilityState)
            || (consistencyState != other.consistencyState) );
   }

   bool operator==(const CombinedTargetState& other) const
   {
      return ( (reachabilityState == other.reachabilityState)
            && (consistencyState == other.consistencyState) );
   }

   template<typename This, typename Ctx>
   static void serialize(This obj, Ctx& ctx)
   {
      ctx
         % obj->reachabilityState
         % obj->consistencyState;
   }
};

/**
 * Extends the CombinedTargetState with a timestamp.
 *
 * Note: Creating objects of this type is expensive due to the Time member; thus, this is intended
 * for internal use by the TargetStateStore only.
 */
struct TargetStateInfo : public CombinedTargetState
{
   Time lastChangedTime; // note: relative time, may not be synced across nodes.

   TargetStateInfo(TargetReachabilityState reachabilityState,
         TargetConsistencyState consistencyState)
      : CombinedTargetState(reachabilityState, consistencyState)
   { }

   TargetStateInfo()
   { }

   TargetStateInfo& operator=(const TargetStateInfo& other)
   {
      if (&other == this)
         return *this;

      reachabilityState = other.reachabilityState;
      consistencyState = other.consistencyState;
      lastChangedTime = other.lastChangedTime;

      return *this;
   }

   /**
    * Assignment from a CombinedTargetState (without timestamp) updates timestamp.
    */
   TargetStateInfo& operator=(const CombinedTargetState& other)
   {
      if (&other == this)
         return *this;

      reachabilityState = other.reachabilityState;
      consistencyState = other.consistencyState;
      lastChangedTime.setToNow();

      return *this;
   }

   /**
    * Comparison with a CombinedTargetState ignores timestamp.
    */
   bool operator!=(const CombinedTargetState& other) const
   {
      return ( (reachabilityState != other.reachabilityState)
            || (consistencyState != other.consistencyState) );
   }

   template<typename This, typename Ctx>
   static void serialize(This obj, Ctx& ctx)
   {
      ctx
         % serdes::base<CombinedTargetState>(obj)
         % obj->lastChangedTime;
   }
};

typedef std::map<uint16_t, CombinedTargetState> TargetStateMap;
typedef TargetStateMap::iterator TargetStateMapIter;
typedef TargetStateMap::const_iterator TargetStateMapCIter;
typedef TargetStateMap::value_type TargetStateMapVal;

typedef std::map<uint16_t, TargetStateInfo> TargetStateInfoMap;
typedef TargetStateInfoMap::iterator TargetStateInfoMapIter;
typedef TargetStateInfoMap::const_iterator TargetStateInfoMapConstIter;

#endif /* TARGETSTATEINFO_H_ */
