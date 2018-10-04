#include "ListDirFromOffsetMsg.h"


const struct NetMessageOps ListDirFromOffsetMsg_Ops = {
   .serializePayload = ListDirFromOffsetMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void ListDirFromOffsetMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   ListDirFromOffsetMsg* thisCast = (ListDirFromOffsetMsg*)this;

   // serverOffset
   Serialization_serializeInt64(ctx, thisCast->serverOffset);

   // maxOutNames
   Serialization_serializeUInt(ctx, thisCast->maxOutNames);

   // EntryInfo
   EntryInfo_serialize(ctx, thisCast->entryInfoPtr);

   // filterDots
   Serialization_serializeBool(ctx, thisCast->filterDots);
}
