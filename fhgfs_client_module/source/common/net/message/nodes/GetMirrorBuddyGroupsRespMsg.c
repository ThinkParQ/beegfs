#include "GetMirrorBuddyGroupsRespMsg.h"

const struct NetMessageOps GetMirrorBuddyGroupsRespMsg_Ops = {
   .serializePayload = _NetMessage_serializeDummy,
   .deserializePayload = GetMirrorBuddyGroupsRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

bool GetMirrorBuddyGroupsRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   GetMirrorBuddyGroupsRespMsg* thisCast = (GetMirrorBuddyGroupsRespMsg*)this;

   // mirrorBuddyGroupIDs
   if(!Serialization_deserializeUInt16ListPreprocess(ctx, &thisCast->rawBuddyGroupIDsList) )
      return false;

   // primaryTargetIDs
   if(!Serialization_deserializeUInt16ListPreprocess(ctx, &thisCast->rawPrimaryTargetIDsList) )
      return false;

   // secondaryTargetIDs
   if(!Serialization_deserializeUInt16ListPreprocess(ctx, &thisCast->rawSecondaryTargetIDsList) )
      return false;

   return true;
}
