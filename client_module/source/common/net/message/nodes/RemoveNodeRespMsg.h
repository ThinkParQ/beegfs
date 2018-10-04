#ifndef REMOVENODERESPMSG_H_
#define REMOVENODERESPMSG_H_

#include <common/net/message/SimpleIntMsg.h>


struct RemoveNodeRespMsg;
typedef struct RemoveNodeRespMsg RemoveNodeRespMsg;

static inline void RemoveNodeRespMsg_init(RemoveNodeRespMsg* this);
static inline void RemoveNodeRespMsg_initFromValue(RemoveNodeRespMsg* this, int value);

struct RemoveNodeRespMsg
{
   SimpleIntMsg simpleIntMsg;
};


void RemoveNodeRespMsg_init(RemoveNodeRespMsg* this)
{
   SimpleIntMsg_init( (SimpleIntMsg*)this, NETMSGTYPE_RemoveNodeResp);
}

void RemoveNodeRespMsg_initFromValue(RemoveNodeRespMsg* this, int value)
{
   SimpleIntMsg_initFromValue( (SimpleIntMsg*)this, NETMSGTYPE_RemoveNodeResp, value);
}

#endif /* REMOVENODERESPMSG_H_ */
