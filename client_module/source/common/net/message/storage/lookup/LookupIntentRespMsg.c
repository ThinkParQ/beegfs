#include "LookupIntentRespMsg.h"


const struct NetMessageOps LookupIntentRespMsg_Ops = {
   .serializePayload   = _NetMessage_serializeDummy,
   .deserializePayload = LookupIntentRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

bool LookupIntentRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   LookupIntentRespMsg* thisCast = (LookupIntentRespMsg*)this;

   // pre-initialize
   thisCast->lookupResult = FhgfsOpsErr_INTERNAL;
   thisCast->createResult = FhgfsOpsErr_INTERNAL;

   // responseFlags
   if(!Serialization_deserializeInt(ctx, &thisCast->responseFlags) )
      return false;

   // lookupResult
   if(!Serialization_deserializeUInt(ctx, &thisCast->lookupResult) )
      return false;

   if(thisCast->responseFlags & LOOKUPINTENTRESPMSG_FLAG_STAT)
   {
      // statResult
      if(!Serialization_deserializeInt(ctx, &thisCast->statResult) )
         return false;

      // StatData
      if(!StatData_deserialize(ctx, &thisCast->statData) )
         return false;
   }

   if(thisCast->responseFlags & LOOKUPINTENTRESPMSG_FLAG_REVALIDATE)
   {
      // revalidateResult
      if(!Serialization_deserializeInt(ctx, &thisCast->revalidateResult) )
         return false;
   }

   if(thisCast->responseFlags & LOOKUPINTENTRESPMSG_FLAG_CREATE)
   {
      // createResult
      if(!Serialization_deserializeInt(ctx, &thisCast->createResult) )
         return false;
   }

   if(thisCast->responseFlags & LOOKUPINTENTRESPMSG_FLAG_OPEN)
   {
      // openResult
      if(!Serialization_deserializeInt(ctx, &thisCast->openResult) )
         return false;

      // fileHandleID
      if(!Serialization_deserializeStrAlign4(ctx,
            &thisCast->fileHandleIDLen, &thisCast->fileHandleID) )
         return false;

      // stripePattern
      if(!StripePattern_deserializePatternPreprocess(ctx,
         &thisCast->patternStart, &thisCast->patternLength) )
         return false;

      // PathInfo
      if(!PathInfo_deserialize(ctx, &thisCast->pathInfo) )
         return false;
   }

   // only try to decode the EntryInfo if either of those was successful
   if ( (thisCast->lookupResult == FhgfsOpsErr_SUCCESS) ||
        (thisCast->createResult == FhgfsOpsErr_SUCCESS) )
   { // entryInfo
      if (unlikely(!EntryInfo_deserialize(ctx, &thisCast->entryInfo) ) )
      {
         printk_fhgfs(KERN_INFO,
            "EntryInfo deserialization failed. LookupResult: %d; CreateResult %d\n",
            thisCast->lookupResult, thisCast->createResult);
         return false;
      }
   }


   return true;
}


