#ifndef REGISTERNODERESPMSG_H
#define REGISTERNODERESPMSG_H

#include <common/net/message/NetMessage.h>

struct RegisterNodeRespMsg;
typedef struct RegisterNodeRespMsg RegisterNodeRespMsg;

static inline void RegisterNodeRespMsg_init(RegisterNodeRespMsg* this);
static inline NumNodeID RegisterNodeRespMsg_getNodeNumID(RegisterNodeRespMsg* this);
static inline int RegisterNodeRespMsg_getGrpcPort(RegisterNodeRespMsg* this);
static inline const char* RegisterNodeRespMsg_getFsUUID(RegisterNodeRespMsg* this);

// virtual functions
extern bool RegisterNodeRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);

struct RegisterNodeRespMsg
{
   NetMessage netMessage;

   NumNodeID nodeNumID;
   unsigned short grpcPort;
   unsigned fsUUIDLen;
   const char* fsUUID;
};

extern const struct NetMessageOps RegisterNodeRespMsg_Ops;

void RegisterNodeRespMsg_init(RegisterNodeRespMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_RegisterNodeResp, &RegisterNodeRespMsg_Ops);
}

NumNodeID RegisterNodeRespMsg_getNodeNumID(RegisterNodeRespMsg* this)
{
   return this->nodeNumID;
}

int RegisterNodeRespMsg_getGrpcPort(RegisterNodeRespMsg* this)
{
   return this->grpcPort;
}

const char* RegisterNodeRespMsg_getFsUUID(RegisterNodeRespMsg* this)
{
   return this->fsUUID;
}

#endif /*REGISTERNODERESPMSG_H*/
