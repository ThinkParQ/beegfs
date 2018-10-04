#include "GetTargetMappingsRespMsg.h"

#include <common/nodes/TargetMapper.h>

static void GetTargetMappingsRespMsg_release(NetMessage* this)
{
   GetTargetMappingsRespMsg* thisCast = (GetTargetMappingsRespMsg*)this;

   BEEGFS_KFREE_LIST(&thisCast->mappings, struct TargetMapping, _list);
}

bool GetTargetMappingsRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx)
{
   GetTargetMappingsRespMsg* thisCast = (GetTargetMappingsRespMsg*)this;

   if (!TargetMappingList_deserialize(ctx, &thisCast->mappings))
      return false;

   return true;
}

const struct NetMessageOps GetTargetMappingsRespMsg_Ops = {
   .serializePayload = _NetMessage_serializeDummy,
   .deserializePayload = GetTargetMappingsRespMsg_deserializePayload,
   .processIncoming = NetMessage_processIncoming,
   .getSupportedHeaderFeatureFlagsMask = NetMessage_getSupportedHeaderFeatureFlagsMask,
   .release = GetTargetMappingsRespMsg_release,
};
