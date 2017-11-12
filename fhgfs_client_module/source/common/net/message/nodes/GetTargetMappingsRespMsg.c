#include "GetTargetMappingsRespMsg.h"


const struct NetMessageOps GetTargetMappingsRespMsg_Ops = {
   .serializePayload = _NetMessage_serializeDummy,
   .deserializePayload = GetTargetMappingsRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

bool GetTargetMappingsRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   GetTargetMappingsRespMsg* thisCast = (GetTargetMappingsRespMsg*)this;

   // targetIDs
   if(!Serialization_deserializeUInt16ListPreprocess(ctx, &thisCast->rawTargetIDsList) )
      return false;

   // nodeIDs
   if(!Serialization_deserializeNumNodeIDListPreprocess(ctx, &thisCast->rawNodeIDsList) )
      return false;

   return true;
}
