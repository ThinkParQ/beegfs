#include "SimpleIntMsg.h"

const struct NetMessageOps SimpleIntMsg_Ops = {
   .serializePayload = SimpleIntMsg_serializePayload,
   .deserializePayload = SimpleIntMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void SimpleIntMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   SimpleIntMsg* thisCast = (SimpleIntMsg*)this;

   Serialization_serializeInt(ctx, thisCast->value);
}

bool SimpleIntMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   SimpleIntMsg* thisCast = (SimpleIntMsg*)this;

   return Serialization_deserializeInt(ctx, &thisCast->value);
}
