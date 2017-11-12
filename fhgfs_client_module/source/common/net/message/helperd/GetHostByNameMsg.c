#include <app/App.h>
#include <common/toolkit/SocketTk.h>
#include "GetHostByNameMsg.h"

const struct NetMessageOps GetHostByNameMsg_Ops = {
   .serializePayload = GetHostByNameMsg_serializePayload,
   .deserializePayload = GetHostByNameMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void GetHostByNameMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   GetHostByNameMsg* thisCast = (GetHostByNameMsg*)this;

   // hostname
   Serialization_serializeStr(ctx, thisCast->hostnameLen, thisCast->hostname);
}

bool GetHostByNameMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   GetHostByNameMsg* thisCast = (GetHostByNameMsg*)this;

   // hostname
   if(!Serialization_deserializeStr(ctx, &thisCast->hostnameLen, &thisCast->hostname) )
      return false;

   return true;
}
