#include "TruncFileMsg.h"


const struct NetMessageOps TruncFileMsg_Ops = {
   .serializePayload = TruncFileMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void TruncFileMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   TruncFileMsg* thisCast = (TruncFileMsg*)this;

   // filesize
   Serialization_serializeInt64(ctx, thisCast->filesize);

   // entryInfo
   EntryInfo_serialize(thisCast->entryInfoPtr, ctx);
}
