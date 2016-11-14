#include "SimpleInt64Msg.h"

const struct NetMessageOps SimpleInt64Msg_Ops = {
   .serializePayload = SimpleInt64Msg_serializePayload,
   .deserializePayload = SimpleInt64Msg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void SimpleInt64Msg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   SimpleInt64Msg* thisCast = (SimpleInt64Msg*)this;

   Serialization_serializeInt64(ctx, thisCast->value);
}

bool SimpleInt64Msg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   SimpleInt64Msg* thisCast = (SimpleInt64Msg*)this;

   return Serialization_deserializeInt64(ctx, &thisCast->value);
}
