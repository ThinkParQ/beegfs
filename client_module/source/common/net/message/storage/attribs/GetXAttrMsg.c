#include "GetXAttrMsg.h"

const struct NetMessageOps GetXAttrMsg_Ops = {
   .serializePayload = GetXAttrMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void GetXAttrMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   GetXAttrMsg* thisCast = (GetXAttrMsg*)this;

   EntryInfo_serialize(ctx, thisCast->entryInfoPtr);
   Serialization_serializeStr(ctx, strlen(thisCast->name), thisCast->name);
   Serialization_serializeInt(ctx, thisCast->size);
}
