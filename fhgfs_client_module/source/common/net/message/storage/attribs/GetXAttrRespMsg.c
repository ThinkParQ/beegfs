#include <common/toolkit/Serialization.h>

#include "GetXAttrRespMsg.h"

const struct NetMessageOps GetXAttrRespMsg_Ops = {
   .serializePayload   = _NetMessage_serializeDummy,
   .deserializePayload = GetXAttrRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

bool GetXAttrRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   GetXAttrRespMsg* thisCast = (GetXAttrRespMsg*)this;

   const char** valueBufPtr = (const char**)&thisCast->valueBuf;

   {
      unsigned valueFieldLen;
      if (!Serialization_deserializeCharArray(ctx, &thisCast->valueBufLen,
          valueBufPtr, &valueFieldLen) )
         return false;
   }

   if (!Serialization_deserializeInt(ctx, &thisCast->size) )
      return false;

   if (!Serialization_deserializeInt(ctx, &thisCast->returnCode) )
      return false;

   return true;
}
