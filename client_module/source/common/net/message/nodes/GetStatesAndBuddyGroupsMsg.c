#include "GetStatesAndBuddyGroupsMsg.h"

static void GetStatesAndBuddyGroupsMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   GetStatesAndBuddyGroupsMsg* thisCast = (GetStatesAndBuddyGroupsMsg*)this;

   Serialization_serializeInt(ctx, thisCast->nodeType);
   NumNodeID_serialize(ctx, &thisCast->requestedByClientID);
}

static bool GetStatesAndBuddyGroupsMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   GetStatesAndBuddyGroupsMsg* thisCast = (GetStatesAndBuddyGroupsMsg*)this;

   bool result =
      Serialization_deserializeInt(ctx, (int32_t*)&thisCast->nodeType)
      && NumNodeID_deserialize(ctx, &thisCast->requestedByClientID);

   return result;
}

const struct NetMessageOps GetStatesAndBuddyGroupsMsg_Ops = {
   .serializePayload = GetStatesAndBuddyGroupsMsg_serializePayload,
   .deserializePayload = GetStatesAndBuddyGroupsMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};
