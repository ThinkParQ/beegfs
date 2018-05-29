#ifndef GETHOSTBYNAMERESPMSG_H_
#define GETHOSTBYNAMERESPMSG_H_

#include <common/net/message/NetMessage.h>


struct GetHostByNameRespMsg;
typedef struct GetHostByNameRespMsg GetHostByNameRespMsg;

static inline void GetHostByNameRespMsg_init(GetHostByNameRespMsg* this);
static inline void GetHostByNameRespMsg_initFromHostAddr(GetHostByNameRespMsg* this,
   const char* hostAddr);

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

/**
 * @param hostAddr just a reference, so do not free it as long as you use this object!
 */
void GetHostByNameRespMsg_initFromHostAddr(GetHostByNameRespMsg* this,
   const char* hostAddr)
{
   GetHostByNameRespMsg_init(this);

   this->hostAddr = hostAddr;
   this->hostAddrLen = strlen(hostAddr);
}

const char* GetHostByNameRespMsg_getHostAddr(GetHostByNameRespMsg* this)
{
   return this->hostAddr;
}

#endif /*GETHOSTBYNAMERESPMSG_H_*/
