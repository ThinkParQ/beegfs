#include "RefreshEntryInfoMsg.h"


const struct NetMessageOps RefreshEntryInfoMsg_Ops = {
   .serializePayload    = RefreshEntryInfoMsg_serializePayload,
   .deserializePayload  = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
   .supportsSequenceNumbers = true,
};

void RefreshEntryInfoMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   RefreshEntryInfoMsg* thisCast = (RefreshEntryInfoMsg*)this;

   // entryInfo
   EntryInfo_serialize(ctx, thisCast->entryInfoPtr);
}
