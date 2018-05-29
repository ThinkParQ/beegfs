#include "SimpleStringMsg.h"

const struct NetMessageOps SimpleStringMsg_Ops = {
   .serializePayload = SimpleStringMsg_serializePayload,
   .deserializePayload = SimpleStringMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void SimpleStringMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   SimpleStringMsg* thisCast = (SimpleStringMsg*)this;

   Serialization_serializeStr(ctx, thisCast->valueLen, thisCast->value);
}

bool SimpleStringMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   SimpleStringMsg* thisCast = (SimpleStringMsg*)this;

   return Serialization_deserializeStr(ctx, &thisCast->valueLen, &thisCast->value);
}
