#include "SimpleMsg.h"

const struct NetMessageOps SimpleMsg_Ops = {
   .serializePayload = SimpleMsg_serializePayload,
   .deserializePayload = SimpleMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void SimpleMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   // nothing to be done here for simple messages
}

bool SimpleMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   // nothing to be done here for simple messages
   return true;
}
