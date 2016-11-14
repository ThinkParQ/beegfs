#include "RegisterNodeRespMsg.h"

const struct NetMessageOps RegisterNodeRespMsg_Ops = {
   .serializePayload = _NetMessage_serializeDummy, // serialization not implemented
   .deserializePayload = RegisterNodeRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

bool RegisterNodeRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   RegisterNodeRespMsg* thisCast = (RegisterNodeRespMsg*)this;

   // nodeNumID
   if(!NumNodeID_deserialize(ctx, &thisCast->nodeNumID) )
      return false;

   return true;
}
