#include "LookupIntentMsg.h"


const struct NetMessageOps LookupIntentMsg_Ops = {
   .serializePayload = LookupIntentMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
   .supportsSequenceNumbers = true,
};

void LookupIntentMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   LookupIntentMsg* thisCast = (LookupIntentMsg*)this;

   // intentFlags
   Serialization_serializeInt(ctx, thisCast->intentFlags);

   // parentInfo
   EntryInfo_serialize(thisCast->parentInfoPtr, ctx);

   // entryName
   Serialization_serializeStrAlign4(ctx, thisCast->entryNameLen, thisCast->entryName);

   if (thisCast->intentFlags & LOOKUPINTENTMSG_FLAG_REVALIDATE)
   {
      // entryInfo
      EntryInfo_serialize(thisCast->entryInfoPtr, ctx);
   }

   if(thisCast->intentFlags & LOOKUPINTENTMSG_FLAG_OPEN)
   {
      // accessFlags
      Serialization_serializeUInt(ctx, thisCast->accessFlags);

      // clientNumID
      NumNodeID_serialize(&thisCast->clientNumID, ctx);
   }

   if(thisCast->intentFlags & LOOKUPINTENTMSG_FLAG_CREATE)
   {
      // userID
      Serialization_serializeUInt(ctx, thisCast->userID);

      // groupID
      Serialization_serializeUInt(ctx, thisCast->groupID);

      // mode
      Serialization_serializeInt(ctx, thisCast->mode);

      // umask
      Serialization_serializeInt(ctx, thisCast->umask);

      // preferredTargets
      Serialization_serializeUInt16List(ctx, thisCast->preferredTargets);
   }
}
