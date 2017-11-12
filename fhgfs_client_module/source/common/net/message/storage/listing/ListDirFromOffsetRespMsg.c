#include "ListDirFromOffsetRespMsg.h"


const struct NetMessageOps ListDirFromOffsetRespMsg_Ops = {
   .serializePayload = _NetMessage_serializeDummy,
   .deserializePayload = ListDirFromOffsetRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

bool ListDirFromOffsetRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   const char* logContext = "ListDirFromOffsetRespMsg deserialization";
   ListDirFromOffsetRespMsg* thisCast = (ListDirFromOffsetRespMsg*)this;

   // newServerOffset
   if(!Serialization_deserializeInt64(ctx, &thisCast->newServerOffset) )
      return false;

   // serverOffsets
   if(!Serialization_deserializeInt64CpyVecPreprocess(ctx, &thisCast->serverOffsetsList) )
      return false;

   // result
   if(!Serialization_deserializeInt(ctx, &thisCast->result) )
      return false;

   // entryTypes
   if(!Serialization_deserializeUInt8VecPreprocess(ctx, &thisCast->entryTypesList) )
      return false;

   // entryIDs
   if (!Serialization_deserializeStrCpyVecPreprocess(ctx, &thisCast->entryIDsList) )
      return false;

   // names
   if(!Serialization_deserializeStrCpyVecPreprocess(ctx, &thisCast->namesList) )
      return false;

   // sanity check for equal list lengths
   if(unlikely(
      (thisCast->entryTypesList.elemCount != thisCast->namesList.elemCount) ||
      (thisCast->entryTypesList.elemCount != thisCast->entryIDsList.elemCount) ||
      (thisCast->entryTypesList.elemCount != thisCast->serverOffsetsList.elemCount) ) )
   {
      printk_fhgfs(KERN_INFO, "%s: Sanity check failed (number of list elements does not match)!\n",
         logContext);
      return false;
   }


   return true;
}
