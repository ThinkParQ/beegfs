#include "ListXAttrMsg.h"

const struct NetMessageOps ListXAttrMsg_Ops = {
   .serializePayload = ListXAttrMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void ListXAttrMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   ListXAttrMsg* thisCast = (ListXAttrMsg*)this;

   EntryInfo_serialize(ctx, thisCast->entryInfoPtr);
   Serialization_serializeInt(ctx, thisCast->size);
}
