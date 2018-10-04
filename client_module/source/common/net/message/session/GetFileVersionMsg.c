#include "GetFileVersionMsg.h"

static void GetFileVersionMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   struct GetFileVersionMsg* msg = container_of(this, struct GetFileVersionMsg, netMessage);

   EntryInfo_serialize(ctx, msg->entryInfo);
}

const struct NetMessageOps GetFileVersionMsg_Ops = {
   .serializePayload = GetFileVersionMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};
