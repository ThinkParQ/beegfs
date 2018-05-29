#include "GetFileVersionRespMsg.h"

static bool GetFileVersionRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   struct GetFileVersionRespMsg* msg = container_of(this, struct GetFileVersionRespMsg, base);

   int result;

   if (!Serialization_deserializeInt(ctx, &result) ||
         !Serialization_deserializeUInt64(ctx, &msg->version))
      return false;

   msg->result = result;
   return true;
}

const struct NetMessageOps GetFileVersionRespMsg_Ops = {
   .serializePayload = _NetMessage_serializeDummy,
   .deserializePayload = GetFileVersionRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};
