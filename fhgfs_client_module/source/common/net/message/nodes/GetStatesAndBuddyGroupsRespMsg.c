#include "GetStatesAndBuddyGroupsRespMsg.h"

const struct NetMessageOps GetStatesAndBuddyGroupsRespMsg_Ops = {
   .serializePayload = _NetMessage_serializeDummy,
   .deserializePayload = GetStatesAndBuddyGroupsRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

bool GetStatesAndBuddyGroupsRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   GetStatesAndBuddyGroupsRespMsg* thisCast = (GetStatesAndBuddyGroupsRespMsg*)this;

   // mirrorBuddyGroupIDs
   if(!Serialization_deserializeUInt16ListPreprocess(ctx, &thisCast->buddyGroupIDsList) )
      return false;

   // primaryTargetIDs
   if(!Serialization_deserializeUInt16ListPreprocess(ctx, &thisCast->primaryTargetIDsList) )
      return false;

   // secondaryTargetIDs
   if(!Serialization_deserializeUInt16ListPreprocess(ctx, &thisCast->secondaryTargetIDsList) )
      return false;

   // targetIDs
   if(!Serialization_deserializeUInt16ListPreprocess(ctx, &thisCast->targetIDsList) )
      return false;

   // targetReachabilityStates
   if(!Serialization_deserializeUInt8ListPreprocess(ctx, &thisCast->targetReachabilityStates) )
      return false;

   // targetConsistencyStates
   if(!Serialization_deserializeUInt8ListPreprocess(ctx, &thisCast->targetConsistencyStates) )
      return false;

   return true;
}


