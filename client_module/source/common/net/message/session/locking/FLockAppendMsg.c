#include "FLockAppendMsg.h"


const struct NetMessageOps FLockAppendMsg_Ops = {
   .serializePayload = FLockAppendMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void FLockAppendMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   FLockAppendMsg* thisCast = (FLockAppendMsg*)this;

   // clientNumID
   NumNodeID_serialize(ctx, &thisCast->clientNumID);

   // clientFD
   Serialization_serializeInt64(ctx, thisCast->clientFD);

   // ownerPID
   Serialization_serializeInt(ctx, thisCast->ownerPID);

   // lockTypeFlags
   Serialization_serializeInt(ctx, thisCast->lockTypeFlags);

   // entryInfo
   EntryInfo_serialize(ctx, thisCast->entryInfoPtr);

   // fileHandleID
   Serialization_serializeStrAlign4(ctx, thisCast->fileHandleIDLen, thisCast->fileHandleID);

   // lockAckID
   Serialization_serializeStrAlign4(ctx, thisCast->lockAckIDLen, thisCast->lockAckID);
}
