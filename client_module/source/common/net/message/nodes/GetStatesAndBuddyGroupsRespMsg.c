#include "GetStatesAndBuddyGroupsRespMsg.h"

static void GetStatesAndBuddyGroupsRespMsg_release(NetMessage* msg)
{
   GetStatesAndBuddyGroupsRespMsg* this = container_of(msg, struct GetStatesAndBuddyGroupsRespMsg,
         netMessage);

   BEEGFS_KFREE_LIST(&this->groups, struct BuddyGroupMapping, _list);
   BEEGFS_KFREE_LIST(&this->states, struct TargetStateMapping, _list);
}

const struct NetMessageOps GetStatesAndBuddyGroupsRespMsg_Ops = {
   .serializePayload = _NetMessage_serializeDummy,
   .deserializePayload = GetStatesAndBuddyGroupsRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
   .release = GetStatesAndBuddyGroupsRespMsg_release,
};

bool GetStatesAndBuddyGroupsRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   GetStatesAndBuddyGroupsRespMsg* thisCast = (GetStatesAndBuddyGroupsRespMsg*)this;

   if (!BuddyGroupMappingList_deserialize(ctx, &thisCast->groups))
      return false;

   if (!TargetStateMappingList_deserialize(ctx, &thisCast->states))
      return false;

   return true;
}


