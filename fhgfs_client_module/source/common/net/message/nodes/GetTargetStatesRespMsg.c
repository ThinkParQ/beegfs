#include "GetTargetStatesRespMsg.h"

const struct NetMessageOps GetTargetStatesRespMsg_Ops = {
   .serializePayload = _NetMessage_serializeDummy,
   .deserializePayload = GetTargetStatesRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

bool GetTargetStatesRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   GetTargetStatesRespMsg* thisCast = (GetTargetStatesRespMsg*)this;

   // targetIDs
   if(!Serialization_deserializeUInt16ListPreprocess(ctx, &thisCast->rawTargetIDsList) )
      return false;

   // reachabilityStates
   if(!Serialization_deserializeUInt8ListPreprocess(ctx, &thisCast->rawReachabilityStates) )
      return false;

   // consistencyStates
   if(!Serialization_deserializeUInt8ListPreprocess(ctx, &thisCast->rawConsistencyStates) )
      return false;

   return true;
}
