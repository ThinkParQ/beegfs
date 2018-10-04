#ifndef GETNODESMSG_H_
#define GETNODESMSG_H_

#include <common/net/message/SimpleIntMsg.h>


struct GetNodesMsg;
typedef struct GetNodesMsg GetNodesMsg;

static inline void GetNodesMsg_init(GetNodesMsg* this);
static inline void GetNodesMsg_initFromValue(GetNodesMsg* this, int nodeType);

struct GetNodesMsg
{
   SimpleIntMsg simpleIntMsg;
};


void GetNodesMsg_init(GetNodesMsg* this)
{
   SimpleIntMsg_init( (SimpleIntMsg*)this, NETMSGTYPE_GetNodes);
}

/**
 * @param nodeType NODETYPE_...
 */
void GetNodesMsg_initFromValue(GetNodesMsg* this, int nodeType)
{
   SimpleIntMsg_initFromValue( (SimpleIntMsg*)this, NETMSGTYPE_GetNodes, nodeType);
}

#endif /* GETNODESMSG_H_ */
