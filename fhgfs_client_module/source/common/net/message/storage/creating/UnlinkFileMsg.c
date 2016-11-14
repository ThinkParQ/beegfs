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
   EntryInfo_serialize(thisCast->parentInfoPtr, ctx);

   // delFileName
   Serialization_serializeStrAlign4(ctx, thisCast->delFileNameLen, thisCast->delFileName);
}
