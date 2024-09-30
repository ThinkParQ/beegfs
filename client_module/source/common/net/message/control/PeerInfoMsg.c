#include "PeerInfoMsg.h"

static void PeerInfoMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   PeerInfoMsg* thisCast = (PeerInfoMsg*)this;

   Serialization_serializeUInt(ctx, thisCast->type);
   NumNodeID_serialize(ctx, &thisCast->id);
}

static bool PeerInfoMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   PeerInfoMsg* thisCast = (PeerInfoMsg*)this;

   unsigned type = 0;
   bool result =
      Serialization_deserializeUInt(ctx, &type)
      && NumNodeID_deserialize(ctx, &thisCast->id);

   thisCast->type = type;
   return result;
}

const struct NetMessageOps PeerInfoMsg_Ops = {
   .serializePayload = PeerInfoMsg_serializePayload,
   .deserializePayload = PeerInfoMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};
