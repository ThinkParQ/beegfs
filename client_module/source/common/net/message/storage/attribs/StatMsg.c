#include "StatMsg.h"


const struct NetMessageOps StatMsg_Ops = {
   .serializePayload = StatMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = StatMsg_getSupportedHeaderFeatureFlagsMask,
};

void StatMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   StatMsg* thisCast = (StatMsg*)this;

   // entryInfo
   EntryInfo_serialize(ctx, thisCast->entryInfoPtr);
}

unsigned StatMsg_getSupportedHeaderFeatureFlagsMask(NetMessage* this)
{
   return STATMSG_FLAG_GET_PARENTINFO;
}
