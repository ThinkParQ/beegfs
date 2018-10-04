#include "UnlinkFileMsg.h"


const struct NetMessageOps UnlinkFileMsg_Ops = {
   .serializePayload = UnlinkFileMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
   .supportsSequenceNumbers = true,
};

void UnlinkFileMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   UnlinkFileMsg* thisCast = (UnlinkFileMsg*)this;

   // parentInf
   EntryInfo_serialize(ctx, thisCast->parentInfoPtr);

   // delFileName
   Serialization_serializeStrAlign4(ctx, thisCast->delFileNameLen, thisCast->delFileName);

   if (this->msgHeader.msgFeatureFlags & UNLINKFILEMSG_FLAG_HAS_EVENT)
      FileEvent_serialize(ctx, thisCast->fileEvent);
}
