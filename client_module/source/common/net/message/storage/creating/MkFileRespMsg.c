#include <common/storage/StorageErrors.h>
#include "MkFileRespMsg.h"


const struct NetMessageOps MkFileRespMsg_Ops = {
   .serializePayload   = _NetMessage_serializeDummy,
   .deserializePayload = MkFileRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

bool MkFileRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   MkFileRespMsg* thisCast = (MkFileRespMsg*)this;

   // result
   if(!Serialization_deserializeUInt(ctx, &thisCast->result) )
      return false;

   if ( (FhgfsOpsErr)thisCast->result == FhgfsOpsErr_SUCCESS)
   { // entryInfo, empty object if result != FhgfsOpsErr_SUCCESS, deserialization would fail then
      if (!EntryInfo_deserialize(ctx, &thisCast->entryInfo) )
         return false;
   }

   return true;
}
