#include "FLockRangeMsg.h"


const struct NetMessageOps FLockRangeMsg_Ops = {
   .serializePayload = FLockRangeMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void FLockRangeMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   FLockRangeMsg* thisCast = (FLockRangeMsg*)this;

   // clientNumID
   NumNodeID_serialize(&thisCast->clientNumID, ctx);

   // start
   Serialization_serializeUInt64(ctx, thisCast->start);

   // end
   Serialization_serializeUInt64(ctx, thisCast->end);

   // ownerPID
   Serialization_serializeInt(ctx, thisCast->ownerPID);

   // lockTypeFlags
   Serialization_serializeInt(ctx, thisCast->lockTypeFlags);

   // entryInfo
   EntryInfo_serialize(thisCast->entryInfoPtr, ctx);

   // fileHandleID
   Serialization_serializeStrAlign4(ctx, thisCast->fileHandleIDLen, thisCast->fileHandleID);

   // lockAckID
   Serialization_serializeStrAlign4(ctx, thisCast->lockAckIDLen, thisCast->lockAckID);
}
