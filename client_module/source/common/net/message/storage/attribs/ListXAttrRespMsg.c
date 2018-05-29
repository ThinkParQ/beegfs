#include <common/toolkit/Serialization.h>

#include "ListXAttrRespMsg.h"

const struct NetMessageOps ListXAttrRespMsg_Ops = {
   .serializePayload   = _NetMessage_serializeDummy,
   .deserializePayload = ListXAttrRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

bool ListXAttrRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   ListXAttrRespMsg* thisCast = (ListXAttrRespMsg*)this;

   { // value
      RawList valueField;

      if(!Serialization_deserializeStrCpyVecPreprocess(ctx, &valueField) )
         return false;

      thisCast->valueElemNum = valueField.elemCount;
      thisCast->valueBuf = (char*) valueField.data;
   }

   // size
   if (!Serialization_deserializeInt(ctx, &thisCast->size) )
      return false;

   // returnCode
   if (!Serialization_deserializeInt(ctx, &thisCast->returnCode) )
      return false;

   return true;
}
