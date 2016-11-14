#include "MkFileMsg.h"


const struct NetMessageOps MkFileMsg_Ops = {
   .serializePayload   = MkFileMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
   .supportsSequenceNumbers = true,
};

void MkFileMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   MkFileMsg* thisCast = (MkFileMsg*)this;

   // userID
   Serialization_serializeUInt(ctx, thisCast->userID);

   // groupID
   Serialization_serializeUInt(ctx, thisCast->groupID);

   // mode
   Serialization_serializeInt(ctx, thisCast->mode);

   // umask
   Serialization_serializeInt(ctx, thisCast->umask);

   // optional stripe hints
   if(NetMessage_isMsgHeaderFeatureFlagSet( (NetMessage*)this, MKFILEMSG_FLAG_STRIPEHINTS) )
   {
      // numtargets
      Serialization_serializeUInt(ctx, thisCast->numtargets);

      // chunksize
      Serialization_serializeUInt(ctx, thisCast->chunksize);
   }

   // parentInfoPtr
   EntryInfo_serialize(thisCast->parentInfoPtr, ctx);

   // newFileName
   Serialization_serializeStrAlign4(ctx, thisCast->newFileNameLen, thisCast->newFileName);

   // preferredTargets
   Serialization_serializeUInt16List(ctx, thisCast->preferredTargets);
}
