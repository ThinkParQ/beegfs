#include "RenameMsg.h"


const struct NetMessageOps RenameMsg_Ops = {
   .serializePayload   = RenameMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
   .supportsSequenceNumbers = true,
};

void RenameMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   RenameMsg* thisCast = (RenameMsg*)this;

   //entryType
   Serialization_serializeUInt(ctx, thisCast->entryType);

   // fromDirInfo
   EntryInfo_serialize(ctx, thisCast->fromDirInfo);

   // toDirInfo
   EntryInfo_serialize(ctx, thisCast->toDirInfo);

   // oldName
   Serialization_serializeStrAlign4(ctx, thisCast->oldNameLen, thisCast->oldName);

   // newName
   Serialization_serializeStrAlign4(ctx, thisCast->newNameLen, thisCast->newName);

   if (this->msgHeader.msgFeatureFlags & RENAMEMSG_FLAG_HAS_EVENT)
      FileEvent_serialize(ctx, thisCast->fileEvent);
}
