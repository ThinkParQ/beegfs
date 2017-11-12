#ifndef AUTHENTICATECHANNELMSG_H_
#define AUTHENTICATECHANNELMSG_H_

#include <common/net/message/SimpleInt64Msg.h>


struct AuthenticateChannelMsg;
typedef struct AuthenticateChannelMsg AuthenticateChannelMsg;


static inline void AuthenticateChannelMsg_init(AuthenticateChannelMsg* this);
static inline void AuthenticateChannelMsg_initFromValue(AuthenticateChannelMsg* this,
   uint64_t authHash);


struct AuthenticateChannelMsg
{
   SimpleInt64Msg simpleInt64Msg;
};


void AuthenticateChannelMsg_init(AuthenticateChannelMsg* this)
{
   SimpleInt64Msg_init( (SimpleInt64Msg*)this, NETMSGTYPE_AuthenticateChannel);
}

void AuthenticateChannelMsg_initFromValue(AuthenticateChannelMsg* this, uint64_t authHash)
{
   SimpleInt64Msg_initFromValue( (SimpleInt64Msg*)this, NETMSGTYPE_AuthenticateChannel, authHash);
}

#endif /* AUTHENTICATECHANNELMSG_H_ */
