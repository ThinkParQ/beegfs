#include "GetNodesRespMsg.h"

const struct NetMessageOps GetNodesRespMsg_Ops = {
   .serializePayload = _NetMessage_serializeDummy,
   .deserializePayload = GetNodesRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};


bool GetNodesRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   GetNodesRespMsg* thisCast = (GetNodesRespMsg*)this;

   // nodeList
   if(!Serialization_deserializeNodeListPreprocess(ctx, &thisCast->rawNodeList) )
      return false;

   // rootNumID
   if(!NumNodeID_deserialize(ctx, &thisCast->rootNumID) )
      return false;

   // rootIsBuddyMirrored
   if(!Serialization_deserializeBool(ctx, &thisCast->rootIsBuddyMirrored) )
      return false;

   return true;
}


