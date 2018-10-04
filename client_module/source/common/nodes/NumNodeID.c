#include <common/toolkit/Serialization.h>
#include "NumNodeID.h"

void NumNodeID_serialize(SerializeCtx* ctx, const NumNodeID* this)
{
   Serialization_serializeUInt(ctx, this->value);
}

bool NumNodeID_deserialize(DeserializeCtx* ctx, NumNodeID* outThis)
{
   if(!Serialization_deserializeUInt(ctx, &(outThis->value) ) )
      return false;

   return true;
}
