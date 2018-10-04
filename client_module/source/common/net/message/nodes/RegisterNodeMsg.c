#include "RegisterNodeMsg.h"

const struct NetMessageOps RegisterNodeMsg_Ops = {
   .serializePayload = RegisterNodeMsg_serializePayload,
   .deserializePayload = _NetMessage_deserializeDummy,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
};

void RegisterNodeMsg_serializePayload(NetMessage* this, SerializeCtx* ctx)
{
   RegisterNodeMsg* thisCast = (RegisterNodeMsg*)this;

   // instanceVersion
   Serialization_serializeUInt64(ctx, thisCast->instanceVersion);

   // nicListVersion
   Serialization_serializeUInt64(ctx, thisCast->nicListVersion);

   // nodeID
   Serialization_serializeStr(ctx, thisCast->nodeIDLen, thisCast->nodeID);

   // nicList
   Serialization_serializeNicList(ctx, thisCast->nicList);

   // nodeType
   Serialization_serializeInt(ctx, thisCast->nodeType);

   // nodeNumID
   NumNodeID_serialize(ctx, &thisCast->nodeNumID);

   // rootNumID
   NumNodeID_serialize(ctx, &thisCast->rootNumID);

   // rootIsBuddyMirrored
   Serialization_serializeBool(ctx, thisCast->rootIsBuddyMirrored);

   // portUDP
   Serialization_serializeUShort(ctx, thisCast->portUDP);

   // portTCP
   Serialization_serializeUShort(ctx, thisCast->portTCP);
}
