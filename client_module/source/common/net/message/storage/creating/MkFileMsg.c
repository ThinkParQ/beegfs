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

   // optional storage pool ID to overide settings in parent directory
   if(NetMessage_isMsgHeaderFeatureFlagSet( (NetMessage*)this, MKFILEMSG_FLAG_STORAGEPOOLID) )
   {
      // storagePoolId
      StoragePoolId_serialize(ctx, &thisCast->storagePoolId);
   }

   // parentInfoPtr
   EntryInfo_serialize(ctx, thisCast->parentInfoPtr);

   // newFileName
   Serialization_serializeStrAlign4(ctx, thisCast->newFileNameLen, thisCast->newFileName);

   // preferredTargets
   Serialization_serializeUInt16List(ctx, thisCast->preferredTargets);

   if (this->msgHeader.msgFeatureFlags & MKFILEMSG_FLAG_HAS_EVENT)
      FileEvent_serialize(ctx, thisCast->fileEvent);
}
