#ifndef NODELISTITER_H_
#define NODELISTITER_H_

#include <common/toolkit/list/PointerListIter.h>
#include <common/Common.h>
#include "NodeList.h"
#include <common/nodes/Node.h>

struct Node;

struct NodeListIter;
typedef struct NodeListIter NodeListIter;

static inline void NodeListIter_init(NodeListIter* this, NodeList* list);
static inline void NodeListIter_next(NodeListIter* this);
static inline struct Node* NodeListIter_value(NodeListIter* this);
static inline bool NodeListIter_end(NodeListIter* this);


struct NodeListIter
{
   PointerListIter pointerListIter;
};


void NodeListIter_init(NodeListIter* this, NodeList* list)
{
   PointerListIter_init( (PointerListIter*)this, (PointerList*)list);
}

void NodeListIter_next(NodeListIter* this)
{
   PointerListIter_next( (PointerListIter*)this);
}

struct Node* NodeListIter_value(NodeListIter* this)
{
   return (struct Node*)PointerListIter_value( (PointerListIter*)this);
}

bool NodeListIter_end(NodeListIter* this)
{
   return PointerListIter_end( (PointerListIter*)this);
}



#endif /* NODELISTITER_H_ */
