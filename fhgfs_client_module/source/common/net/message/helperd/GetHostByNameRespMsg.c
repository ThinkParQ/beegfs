#include <app/App.h>
#include <common/toolkit/SocketTk.h>
#include "GetHostByNameRespMsg.h"

const struct NetMessageOps GetHostByNameRespMsg_Ops = {
   .serializePayload = _NetMessage_serializeDummy,
   .deserializePayload = GetHostByNameRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

bool GetHostByNameRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   GetHostByNameRespMsg* thisCast = (GetHostByNameRespMsg*)this;

   // hostAddr
   if(!Serialization_deserializeStr(ctx, &thisCast->hostAddrLen, &thisCast->hostAddr) )
      return false;

   return true;
}
