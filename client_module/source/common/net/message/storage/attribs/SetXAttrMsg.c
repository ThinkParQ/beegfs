#include "SetXAttrMsg.h"

const struct NetMessageOps SetXAttrMsg_Ops = {
   .serializePayload = SetXAttrMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
   .supportsSequenceNumbers = true,
};

void SetXAttrMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   SetXAttrMsg* thisCast = (SetXAttrMsg*)this;

   EntryInfo_serialize(ctx, thisCast->entryInfoPtr);
   Serialization_serializeStr(ctx, strlen(thisCast->name), thisCast->name);
   Serialization_serializeCharArray(ctx, thisCast->size, thisCast->value);
   Serialization_serializeInt(ctx, thisCast->flags);
}
