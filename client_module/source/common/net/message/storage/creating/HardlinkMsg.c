#include "HardlinkMsg.h"


const struct NetMessageOps HardlinkMsg_Ops = {
   .serializePayload   = HardlinkMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
   .supportsSequenceNumbers = true,
};

void HardlinkMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   HardlinkMsg* thisCast = (HardlinkMsg*)this;

   // fromInfo
   EntryInfo_serialize(ctx, thisCast->fromInfo);

   // toDirInfo
   EntryInfo_serialize(ctx, thisCast->toDirInfo);

   // toName
   Serialization_serializeStrAlign4(ctx, thisCast->toNameLen, thisCast->toName);

   // fromDirInfo
   EntryInfo_serialize(ctx, thisCast->fromDirInfo);

   // fromName
   Serialization_serializeStrAlign4(ctx, thisCast->fromNameLen, thisCast->fromName);

   if (this->msgHeader.msgFeatureFlags & HARDLINKMSG_FLAG_HAS_EVENT)
      FileEvent_serialize(ctx, thisCast->fileEvent);
}
