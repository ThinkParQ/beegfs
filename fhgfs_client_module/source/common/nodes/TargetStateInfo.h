#ifndef TARGETSTATEINFO_H
#define TARGETSTATEINFO_H

#include <common/Common.h>
#include <common/toolkit/Time.h>

/* make sure to keep this in sync with TargetState in common lib */
enum TargetReachabilityState
{
   TargetReachabilityState_ONLINE,
   TargetReachabilityState_POFFLINE, // probably offline
   TargetReachabilityState_OFFLINE
};

typedef enum TargetReachabilityState TargetReachabilityState;

enum TargetConsistencyState
{
   TargetConsistencyState_GOOD,
   TargetConsistencyState_NEEDS_RESYNC,
   TargetConsistencyState_BAD
};

typedef enum TargetConsistencyState TargetConsistencyState;

struct CombinedTargetState;
typedef struct CombinedTargetState CombinedTargetState;

struct CombinedTargetState
{
   TargetReachabilityState reachabilityState;
   TargetConsistencyState consistencyState;
};

struct TargetStateInfo;
typedef struct TargetStateInfo TargetStateInfo;

static inline void TargetStateInfo_initFromState(TargetStateInfo* this, CombinedTargetState state);
static inline TargetStateInfo* TargetStateInfo_construct(void);
static inline TargetStateInfo* TargetStateInfo_constructFromState(CombinedTargetState state);
static inline void TargetStateInfo_uninit(TargetStateInfo* this);
static inline void TargetStateInfo_destruct(TargetStateInfo* this);

struct TargetStateInfo
{
   CombinedTargetState state;
   Time lastChangedTime; // NOTE: this is not used/updated at the moment!
};

void TargetStateInfo_initFromState(TargetStateInfo* this, CombinedTargetState state)
{
   Time_init(&this->lastChangedTime);
   this->state = state;
}

struct TargetStateInfo* TargetStateInfo_construct(void)
{
   struct TargetStateInfo* this = (TargetStateInfo*) os_kmalloc(sizeof(*this) );

   if (likely(this) )
   {
      CombinedTargetState state = { TargetReachabilityState_OFFLINE, TargetConsistencyState_GOOD };
      TargetStateInfo_initFromState(this, state);
   }

   return this;
}

struct TargetStateInfo* TargetStateInfo_constructFromState(CombinedTargetState state)
{
   struct TargetStateInfo* this = (TargetStateInfo*) os_kmalloc(sizeof(*this) );

   if (likely(this) )
      TargetStateInfo_initFromState(this, state);

   return this;
}

void TargetStateInfo_uninit(TargetStateInfo* this)
{
   Time_uninit(&this->lastChangedTime);
}

void TargetStateInfo_destruct(TargetStateInfo* this)
{
   TargetStateInfo_uninit(this);

   kfree(this);
}

#endif /* TARGETSTATEINFO_H */
