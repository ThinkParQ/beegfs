#ifdef BEEGFS_NVFS
#include "ReadLocalFileRDMAMsg.h"

const struct NetMessageOps ReadLocalFileRDMAMsg_Ops = {
   .serializePayload   = ReadLocalFileRDMAMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void ReadLocalFileRDMAMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   ReadLocalFileRDMAMsg* thisCast = (ReadLocalFileRDMAMsg*)this;

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

   // RDMA info
   RdmaInfo_serialize(ctx, thisCast->rdmap);
}
#endif /* BEEGFS_NVFS */
