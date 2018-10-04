#include "FSyncLocalFileMsg.h"


const struct NetMessageOps FSyncLocalFileMsg_Ops = {
   .serializePayload = FSyncLocalFileMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void FSyncLocalFileMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   FSyncLocalFileMsg* thisCast = (FSyncLocalFileMsg*)this;

   // clientNumID
   NumNodeID_serialize(ctx, &thisCast->clientNumID);

   // fileHandleID
   Serialization_serializeStrAlign4(ctx, thisCast->fileHandleIDLen, thisCast->fileHandleID);

   // targetID
   Serialization_serializeUShort(ctx, thisCast->targetID);
}
