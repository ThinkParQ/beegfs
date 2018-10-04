#ifndef GETTARGETMAPPINGSRESPMSG_H_
#define GETTARGETMAPPINGSRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/Common.h>

/**
 * Note: This message can only be received/deserialized (send/serialization not implemented)
 */


struct GetTargetMappingsRespMsg;
typedef struct GetTargetMappingsRespMsg GetTargetMappingsRespMsg;

static inline void GetTargetMappingsRespMsg_init(GetTargetMappingsRespMsg* this);

// virtual functions
extern bool GetTargetMappingsRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);


struct GetTargetMappingsRespMsg
{
   NetMessage netMessage;

   struct list_head mappings; /* TargetMapping */
};

extern const struct NetMessageOps GetTargetMappingsRespMsg_Ops;

void GetTargetMappingsRespMsg_init(GetTargetMappingsRespMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_GetTargetMappingsResp,
      &GetTargetMappingsRespMsg_Ops);

   INIT_LIST_HEAD(&this->mappings);
}

#endif /* GETTARGETMAPPINGSRESPMSG_H_ */
