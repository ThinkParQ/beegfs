#ifndef SETCHANNELDIRECTMSG_H_
#define SETCHANNELDIRECTMSG_H_

#include <common/net/message/SimpleIntMsg.h>


struct SetChannelDirectMsg;
typedef struct SetChannelDirectMsg SetChannelDirectMsg;

static inline void SetChannelDirectMsg_init(SetChannelDirectMsg* this);
static inline void SetChannelDirectMsg_initFromValue(SetChannelDirectMsg* this, int value);

struct SetChannelDirectMsg
{
   SimpleIntMsg simpleIntMsg;
};


void SetChannelDirectMsg_init(SetChannelDirectMsg* this)
{
   SimpleIntMsg_init( (SimpleIntMsg*)this, NETMSGTYPE_SetChannelDirect);
}

void SetChannelDirectMsg_initFromValue(SetChannelDirectMsg* this, int value)
{
   SimpleIntMsg_initFromValue( (SimpleIntMsg*)this, NETMSGTYPE_SetChannelDirect, value);
}

#endif /*SETCHANNELDIRECTMSG_H_*/
