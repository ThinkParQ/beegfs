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
   EntryInfo_serialize(thisCast->parentInfo, ctx);

   // delDirName
   Serialization_serializeStrAlign4(ctx, thisCast->delDirNameLen, thisCast->delDirName);
}
