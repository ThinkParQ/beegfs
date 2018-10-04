#include "OpenFileMsg.h"


const struct NetMessageOps OpenFileMsg_Ops = {
   .serializePayload = OpenFileMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
   .supportsSequenceNumbers = true,
};

void OpenFileMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   OpenFileMsg* thisCast = (OpenFileMsg*)this;

   // clientNumID
   NumNodeID_serialize(ctx, &thisCast->clientNumID);

   // accessFlags
   Serialization_serializeUInt(ctx, thisCast->accessFlags);

   // entryInfo
   EntryInfo_serialize(ctx, thisCast->entryInfoPtr);

   if (this->msgHeader.msgFeatureFlags & OPENFILEMSG_FLAG_HAS_EVENT)
      FileEvent_serialize(ctx, thisCast->fileEvent);
}
