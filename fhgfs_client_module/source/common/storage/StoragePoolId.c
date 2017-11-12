#include "StoragePoolId.h"

#include <common/toolkit/Serialization.h>

void StoragePoolId_serialize(const StoragePoolId* this, SerializeCtx* ctx)
{
   Serialization_serializeUShort(ctx, this->value);
}

bool StoragePoolId_deserialize(DeserializeCtx* ctx, StoragePoolId* outThis)
{
   if(!Serialization_deserializeUShort(ctx, &(outThis->value) ) )
      return false;

   return true;
}
