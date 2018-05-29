#include "StatStoragePathRespMsg.h"


const struct NetMessageOps StatStoragePathRespMsg_Ops = {
   .serializePayload = _NetMessage_serializeDummy,
   .deserializePayload = StatStoragePathRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

bool StatStoragePathRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   StatStoragePathRespMsg* thisCast = (StatStoragePathRespMsg*)this;

   // result
   if(!Serialization_deserializeInt(ctx, &thisCast->result) )
      return false;

   // sizeTotal
   if(!Serialization_deserializeInt64(ctx, &thisCast->sizeTotal) )
      return false;

   // sizeFree
   if(!Serialization_deserializeInt64(ctx, &thisCast->sizeFree) )
      return false;

   // inodesTotal
   if(!Serialization_deserializeInt64(ctx, &thisCast->inodesTotal) )
      return false;

   // inodesFree
   if(!Serialization_deserializeInt64(ctx, &thisCast->inodesFree) )
      return false;

   return true;
}
