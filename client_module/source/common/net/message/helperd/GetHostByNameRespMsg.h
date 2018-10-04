#ifndef GETHOSTBYNAMERESPMSG_H_
#define GETHOSTBYNAMERESPMSG_H_

#include <common/net/message/NetMessage.h>


struct GetHostByNameRespMsg;
typedef struct GetHostByNameRespMsg GetHostByNameRespMsg;

static inline void GetHostByNameRespMsg_init(GetHostByNameRespMsg* this);

// virtual functions
extern bool GetHostByNameRespMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);

// getters & setters
static inline const char* GetHostByNameRespMsg_getHostAddr(GetHostByNameRespMsg* this);


struct GetHostByNameRespMsg
{
   NetMessage netMessage;

   unsigned hostAddrLen;
   const char* hostAddr;
};

extern const struct NetMessageOps GetHostByNameRespMsg_Ops;

void GetHostByNameRespMsg_init(GetHostByNameRespMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_GetHostByNameResp, &GetHostByNameRespMsg_Ops);
}

const char* GetHostByNameRespMsg_getHostAddr(GetHostByNameRespMsg* this)
{
   return this->hostAddr;
}

#endif /*GETHOSTBYNAMERESPMSG_H_*/
