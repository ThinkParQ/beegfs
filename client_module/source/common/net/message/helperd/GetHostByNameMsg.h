#ifndef GETHOSTBYNAMEMSG_H_
#define GETHOSTBYNAMEMSG_H_

#include <common/net/message/NetMessage.h>


struct GetHostByNameMsg;
typedef struct GetHostByNameMsg GetHostByNameMsg;

static inline void GetHostByNameMsg_init(GetHostByNameMsg* this);
static inline void GetHostByNameMsg_initFromHostname(GetHostByNameMsg* this,
   const char* hostname);

// virtual functions
extern void GetHostByNameMsg_serializePayload(NetMessage* this, SerializeCtx* ctx);
extern bool GetHostByNameMsg_deserializePayload(NetMessage* this, DeserializeCtx* ctx);

struct GetHostByNameMsg
{
   NetMessage netMessage;

   unsigned hostnameLen;
   const char* hostname;
};

extern const struct NetMessageOps GetHostByNameMsg_Ops;

void GetHostByNameMsg_init(GetHostByNameMsg* this)
{
   NetMessage_init(&this->netMessage, NETMSGTYPE_GetHostByName, &GetHostByNameMsg_Ops);
}

/**
 * @param hostname just a reference, so do not free it as long as you use this object!
 */
void GetHostByNameMsg_initFromHostname(GetHostByNameMsg* this,
   const char* hostname)
{
   GetHostByNameMsg_init(this);

   this->hostname = hostname;
   this->hostnameLen = strlen(hostname);
}

#endif /*GETHOSTBYNAMEMSG_H_*/
