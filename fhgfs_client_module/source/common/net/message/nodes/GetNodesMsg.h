#ifndef GETNODESMSG_H_
#define GETNODESMSG_H_

#include <common/net/message/SimpleIntMsg.h>


struct GetNodesMsg;
typedef struct GetNodesMsg GetNodesMsg;

static inline void GetNodesMsg_init(GetNodesMsg* this);
static inline void GetNodesMsg_initFromValue(GetNodesMsg* this, int nodeType);

// getters & setters
static inline int GetNodesMsg_getValue(GetNodesMsg* this);

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

int GetNodesMsg_getValue(GetNodesMsg* this)
{
   return SimpleIntMsg_getValue( (SimpleIntMsg*)this);
}

#endif /* GETNODESMSG_H_ */
