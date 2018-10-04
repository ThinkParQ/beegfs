#include "ReadLocalFileV2Msg.h"


const struct NetMessageOps ReadLocalFileV2Msg_Ops = {
   .serializePayload   = ReadLocalFileV2Msg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void ReadLocalFileV2Msg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   ReadLocalFileV2Msg* thisCast = (ReadLocalFileV2Msg*)this;

   // offset
   Serialization_serializeInt64(ctx, thisCast->offset);

   // count
   Serialization_serializeInt64(ctx, thisCast->count);

   // accessFlags
   Serialization_serializeUInt(ctx, thisCast->accessFlags);

   // fileHandleID
   Serialization_serializeStrAlign4(ctx, thisCast->fileHandleIDLen, thisCast->fileHandleID);

   // clientNumID
   NumNodeID_serialize(ctx, &thisCast->clientNumID);

   // pathInfo
   PathInfo_serialize(ctx, thisCast->pathInfoPtr);

   // targetID
   Serialization_serializeUShort(ctx, thisCast->targetID);
}
