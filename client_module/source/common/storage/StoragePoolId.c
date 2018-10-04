#include "StoragePoolId.h"

#include <common/toolkit/Serialization.h>

void StoragePoolId_serialize(SerializeCtx* ctx, const StoragePoolId* this)
{
   Serialization_serializeUShort(ctx, this->value);
}

bool StoragePoolId_deserialize(DeserializeCtx* ctx, StoragePoolId* outThis)
{
   if(!Serialization_deserializeUShort(ctx, &(outThis->value) ) )
      return false;

   return true;
}
