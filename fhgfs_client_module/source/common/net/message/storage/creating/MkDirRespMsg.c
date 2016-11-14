#include "MkDirRespMsg.h"
#include <common/storage/StorageErrors.h>

const struct NetMessageOps MkDirRespMsg_Ops = {
   .serializePayload   = _NetMessage_serializeDummy,
   .deserializePayload = MkDirRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

bool MkDirRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   MkDirRespMsg* thisCast = (MkDirRespMsg*)this;

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
