#include "SimpleUInt16Msg.h"

const struct NetMessageOps SimpleUInt16Msg_Ops = {
   .serializePayload = SimpleUInt16Msg_serializePayload,
   .deserializePayload = SimpleUInt16Msg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void SimpleUInt16Msg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   SimpleUInt16Msg* thisCast = (SimpleUInt16Msg*)this;

   Serialization_serializeUShort(ctx, thisCast->value);
}

bool SimpleUInt16Msg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   SimpleUInt16Msg* thisCast = (SimpleUInt16Msg*)this;

   return Serialization_deserializeUShort(ctx, &thisCast->value);
}
