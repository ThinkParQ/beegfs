#ifdef BEEGFS_NVFS
#include "WriteLocalFileRDMAMsg.h"
#include "WriteLocalFileMsg.h"

const struct NetMessageOps WriteLocalFileRDMAMsg_Ops = {
   .serializePayload = WriteLocalFileRDMAMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void WriteLocalFileRDMAMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   WriteLocalFileRDMAMsg* thisCast = (WriteLocalFileRDMAMsg*)this;

   // offset
   Serialization_serializeInt64(ctx, thisCast->offset);

   // count
   Serialization_serializeInt64(ctx, thisCast->count);

   // accessFlags
   Serialization_serializeUInt(ctx, thisCast->accessFlags);

   if(NetMessage_isMsgHeaderFeatureFlagSet(this, WRITELOCALFILEMSG_FLAG_USE_QUOTA))
   {
      // userID
      Serialization_serializeUInt(ctx, thisCast->userID);

      // groupID
      Serialization_serializeUInt(ctx, thisCast->groupID);
   }

   // fileHandleID
   Serialization_serializeStrAlign4(ctx, thisCast->fileHandleIDLen, thisCast->fileHandleID);

   // clientNumID
   NumNodeID_serialize(ctx, &thisCast->clientNumID);

   // pathInfo
   PathInfo_serialize(ctx, thisCast->pathInfo);

   // targetID
   Serialization_serializeUShort(ctx, thisCast->targetID);

   // RDMA info
   RdmaInfo_serialize(ctx, thisCast->rdmap);
}
#endif /* BEEGFS_NVFS */
