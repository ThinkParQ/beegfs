#include "OpenFileRespMsg.h"


const struct NetMessageOps OpenFileRespMsg_Ops = {
   .serializePayload    = _NetMessage_serializeDummy,
   .deserializePayload  = OpenFileRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

bool OpenFileRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   OpenFileRespMsg* thisCast = (OpenFileRespMsg*)this;

   // result
   if(!Serialization_deserializeInt(ctx, &thisCast->result) )
      return false;

   // fileHandleID
   if(!Serialization_deserializeStrAlign4(ctx, &thisCast->fileHandleIDLen,
         &thisCast->fileHandleID) )
      return false;

   // pathInfo
   if (!PathInfo_deserialize(ctx, &thisCast->pathInfo) )
      return false;

   // stripePattern
   if(!StripePattern_deserializePatternPreprocess(ctx,
      &thisCast->patternStart, &thisCast->patternLength) )
      return false;

   if (!Serialization_deserializeUInt64(ctx, &thisCast->fileVersion))
      return false;

   return true;
}
