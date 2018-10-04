#include "Types.h"

SERDES_DEFINE_SERIALIZERS_SIMPLE(, TargetMapping, struct TargetMapping,
      (targetID,  , Serialization, UShort),
      (nodeID,   &, NumNodeID, ))
SERDES_DEFINE_LIST_SERIALIZERS(, TargetMappingList, struct TargetMapping,
      TargetMapping, (void), _list, false)


SERDES_DEFINE_SERIALIZERS_SIMPLE(, TargetPoolMapping, struct TargetPoolMapping,
      (targetID,  , Serialization, UShort),
      (poolID,   &, StoragePoolId, ))
SERDES_DEFINE_LIST_SERIALIZERS(, TargetPoolMappingList, struct TargetPoolMapping,
      TargetPoolMapping, (void), _list, false)


SERDES_DEFINE_SERIALIZERS_SIMPLE(, BuddyGroupMapping, struct BuddyGroupMapping,
      (groupID,           , Serialization, UShort),
      (primaryTargetID,   , Serialization, UShort),
      (secondaryTargetID, , Serialization, UShort))
SERDES_DEFINE_LIST_SERIALIZERS(, BuddyGroupMappingList, struct BuddyGroupMapping,
      BuddyGroupMapping, (void), _list, false)


SERDES_DEFINE_SERIALIZERS_SIMPLE(, TargetStateMapping, struct TargetStateMapping,
      (targetID,          , Serialization, UShort),
      (reachabilityState, , TargetReachabilityState, ),
      (consistencyState,  , TargetConsistencyState, ))
SERDES_DEFINE_LIST_SERIALIZERS(, TargetStateMappingList, struct TargetStateMapping,
      TargetStateMapping, (void), _list, false)
