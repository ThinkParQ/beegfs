#include "CloseFileMsg.h"


const struct NetMessageOps CloseFileMsg_Ops = {
   .serializePayload = CloseFileMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
   .supportsSequenceNumbers = true,
};

void CloseFileMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   CloseFileMsg* thisCast = (CloseFileMsg*)this;

   // clientNumID
   NumNodeID_serialize(ctx, &thisCast->clientNumID);

   // fileHandleID
   Serialization_serializeStrAlign4(ctx, thisCast->fileHandleIDLen, thisCast->fileHandleID);

   // entryInfo
   EntryInfo_serialize(ctx, thisCast->entryInfoPtr);

   // maxUsedNodeIndex
   Serialization_serializeInt(ctx, thisCast->maxUsedNodeIndex);

   if (this->msgHeader.msgFeatureFlags & CLOSEFILEMSG_FLAG_HAS_EVENT)
      FileEvent_serialize(ctx, thisCast->fileEvent);
}
