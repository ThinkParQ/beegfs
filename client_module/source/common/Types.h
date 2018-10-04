#ifndef BEEGFS_TYPES_H_
#define BEEGFS_TYPES_H_

#include <common/storage/StoragePoolId.h>
#include <common/toolkit/Serialization.h>

struct TargetMapping
{
   uint16_t targetID;
   NumNodeID nodeID;

/* private: */
   union {
      struct rb_node _node; /* for use by TargetMapper */
      struct list_head _list; /* for use by de/serialized lists */
   };
};

SERDES_DECLARE_SERIALIZERS(TargetMapping, struct TargetMapping)
SERDES_DECLARE_LIST_SERIALIZERS(TargetMappingList, struct TargetMapping)

/* make sure to keep this in sync with TargetState in common lib */
enum TargetReachabilityState
{
   TargetReachabilityState_ONLINE,
   TargetReachabilityState_POFFLINE, // probably offline
   TargetReachabilityState_OFFLINE
};

typedef enum TargetReachabilityState TargetReachabilityState;

SERDES_DEFINE_ENUM_SERIALIZERS(TargetReachabilityState, enum TargetReachabilityState,
      uint8_t, UInt8)

enum TargetConsistencyState
{
   TargetConsistencyState_GOOD,
   TargetConsistencyState_NEEDS_RESYNC,
   TargetConsistencyState_BAD
};

typedef enum TargetConsistencyState TargetConsistencyState;

SERDES_DEFINE_ENUM_SERIALIZERS(TargetConsistencyState, enum TargetConsistencyState, uint8_t, UInt8)

struct CombinedTargetState
{
   TargetReachabilityState reachabilityState;
   TargetConsistencyState consistencyState;
};

typedef struct CombinedTargetState CombinedTargetState;

struct TargetStateInfo
{
   uint16_t targetID;
   struct CombinedTargetState state;

/* private */
   struct rb_node _node;
};

typedef struct TargetStateInfo TargetStateInfo;

struct TargetPoolMapping
{
   uint16_t targetID;
   StoragePoolId poolID;

/* private: */
   struct list_head _list; /* for de/serialized lists */
};

SERDES_DECLARE_SERIALIZERS(TargetPoolMapping, struct TargetPoolMapping)
SERDES_DECLARE_LIST_SERIALIZERS(TargetPoolMappingList, struct TargetPoolMapping)


struct BuddyGroupMapping
{
   uint16_t groupID;
   uint16_t primaryTargetID;
   uint16_t secondaryTargetID;

/* private: */
   struct list_head _list; /* for de/serialized lists */
};

SERDES_DECLARE_SERIALIZERS(BuddyGroupMapping, struct BuddyGroupMapping)
SERDES_DECLARE_LIST_SERIALIZERS(BuddyGroupMappingList, struct BuddyGroupMapping)


struct TargetStateMapping
{
   uint16_t targetID;
   TargetReachabilityState reachabilityState;
   TargetConsistencyState consistencyState;

/* private: */
   struct list_head _list; /* for de/serialized lists */
};

SERDES_DECLARE_SERIALIZERS(TargetStateMapping, struct TargetStateMapping)
SERDES_DECLARE_LIST_SERIALIZERS(TargetStateMappingList, struct TargetStateMapping)

#endif
