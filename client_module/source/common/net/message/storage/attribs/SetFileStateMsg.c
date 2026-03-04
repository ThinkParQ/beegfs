#include "SetFileStateMsg.h"

const struct NetMessageOps SetFileStateMsg_Ops = {
   .serializePayload = SetFileStateMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
   .supportsSequenceNumbers = true,
};

void SetFileStateMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   SetFileStateMsg* thisCast = (SetFileStateMsg*)this;

   EntryInfo_serialize(ctx, thisCast->entryInfoPtr);
   Serialization_serializeUInt8(ctx, thisCast->state);
}