#ifndef HEARTBEATREQUESTMSGEX_H_
#define HEARTBEATREQUESTMSGEX_H_

#include "../SimpleMsg.h"


struct HeartbeatRequestMsgEx;
typedef struct HeartbeatRequestMsgEx HeartbeatRequestMsgEx;

static inline void HeartbeatRequestMsgEx_init(HeartbeatRequestMsgEx* this);

// virtual functions
extern bool __HeartbeatRequestMsgEx_processIncoming(NetMessage* this, struct App* app,
   fhgfs_sockaddr_in* fromAddr, struct Socket* sock, char* respBuf, size_t bufLen);


struct HeartbeatRequestMsgEx
{
   SimpleMsg simpleMsg;
};

extern const struct NetMessageOps HeartbeatRequestMsgEx_Ops;

void HeartbeatRequestMsgEx_init(HeartbeatRequestMsgEx* this)
{
   SimpleMsg_init(&this->simpleMsg, NETMSGTYPE_HeartbeatRequest);
   this->simpleMsg.netMessage.ops = &HeartbeatRequestMsgEx_Ops;
}

#endif /* HEARTBEATREQUESTMSGEX_H_ */
