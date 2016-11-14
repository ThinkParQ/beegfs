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
   EntryInfo_serialize(thisCast->fromDirInfo, ctx);

   // toDirInfo
   EntryInfo_serialize(thisCast->toDirInfo, ctx);

   // oldName
   Serialization_serializeStrAlign4(ctx, thisCast->oldNameLen, thisCast->oldName);

   // newName
   Serialization_serializeStrAlign4(ctx, thisCast->newNameLen, thisCast->newName);
}
