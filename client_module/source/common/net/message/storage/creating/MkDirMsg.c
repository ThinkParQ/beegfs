#include "MkDirMsg.h"


const struct NetMessageOps MkDirMsg_Ops = {
   .serializePayload = MkDirMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
   .supportsSequenceNumbers = true,
};

void MkDirMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   MkDirMsg* thisCast = (MkDirMsg*)this;

   // userID
   Serialization_serializeUInt(ctx, thisCast->userID);

   // groupID
   Serialization_serializeUInt(ctx, thisCast->groupID);

   // mode
   Serialization_serializeInt(ctx, thisCast->mode);

   // umask
   Serialization_serializeInt(ctx, thisCast->umask);

   // parentInfo
   EntryInfo_serialize(ctx, thisCast->parentInfo);

   // newDirName
   Serialization_serializeStrAlign4(ctx, thisCast->newDirNameLen, thisCast->newDirName);

   // preferredNodes
   Serialization_serializeUInt16List(ctx, thisCast->preferredNodes);

   if (this->msgHeader.msgFeatureFlags & MKDIRMSG_FLAG_HAS_EVENT)
      FileEvent_serialize(ctx, thisCast->fileEvent);
}
