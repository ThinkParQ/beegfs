#include "SimpleIntStringMsg.h"

const struct NetMessageOps SimpleIntStringMsg_Ops = {
   .serializePayload = SimpleIntStringMsg_serializePayload,
   .deserializePayload = SimpleIntStringMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void SimpleIntStringMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   SimpleIntStringMsg* thisCast = (SimpleIntStringMsg*)this;

   Serialization_serializeInt(ctx, thisCast->intValue);
   Serialization_serializeStr(ctx, thisCast->strValueLen, thisCast->strValue);
}

bool SimpleIntStringMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   SimpleIntStringMsg* thisCast = (SimpleIntStringMsg*)this;

   // intValue
   if(!Serialization_deserializeInt(ctx, &thisCast->intValue) )
      return false;

   // strValue
   if(!Serialization_deserializeStr(ctx, &thisCast->strValueLen, &thisCast->strValue) )
      return false;

   return true;
}
