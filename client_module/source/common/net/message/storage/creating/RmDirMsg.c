#include "RmDirMsg.h"


const struct NetMessageOps RmDirMsg_Ops = {
   .serializePayload = RmDirMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
   .supportsSequenceNumbers = true,
};

void RmDirMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   RmDirMsg* thisCast = (RmDirMsg*)this;

   // parentInfo
   EntryInfo_serialize(ctx, thisCast->parentInfo);

   // delDirName
   Serialization_serializeStrAlign4(ctx, thisCast->delDirNameLen, thisCast->delDirName);

   if (this->msgHeader.msgFeatureFlags & RMDIRMSG_FLAG_HAS_EVENT)
      FileEvent_serialize(ctx, thisCast->fileEvent);
}
