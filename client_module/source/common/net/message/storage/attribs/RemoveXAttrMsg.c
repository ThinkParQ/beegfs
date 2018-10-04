#include "RemoveXAttrMsg.h"

const struct NetMessageOps RemoveXAttrMsg_Ops = {
   .serializePayload = RemoveXAttrMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
   .supportsSequenceNumbers = true,
};

void RemoveXAttrMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   RemoveXAttrMsg* thisCast = (RemoveXAttrMsg*)this;

   EntryInfo_serialize(ctx, thisCast->entryInfoPtr);
   Serialization_serializeStr(ctx, strlen(thisCast->name), thisCast->name);
}
