#include "StatRespMsg.h"


const struct NetMessageOps StatRespMsg_Ops = {
   .serializePayload   = _NetMessage_serializeDummy,
   .deserializePayload = StatRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = StatRespMsg_getSupportedHeaderFeatureFlagsMask,
};

bool StatRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   StatRespMsg* thisCast = (StatRespMsg*)this;

   // result
   if(!Serialization_deserializeInt(ctx, &thisCast->result) )
      return false;

   // statData
   if(!StatData_deserialize(ctx, &thisCast->statData) )
      return false;

   if(NetMessage_isMsgHeaderFeatureFlagSet(this, STATRESPMSG_FLAG_HAS_PARENTINFO) )
   {
      {  // parentEntryID
         unsigned strLen;

         if (!Serialization_deserializeStrAlign4(ctx, &strLen, &thisCast->parentEntryID) )
            return false;
      }

      // parentNodeID
      if(!NumNodeID_deserialize(ctx, &thisCast->parentNodeID) )
         return false;
   }


   return true;
}

unsigned StatRespMsg_getSupportedHeaderFeatureFlagsMask(NetMessage* this)
{
   return STATRESPMSG_FLAG_HAS_PARENTINFO;
}
