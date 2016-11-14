#ifndef NODELIST_H_
#define NODELIST_H_

#include <common/toolkit/list/PointerList.h>
#include <common/Common.h>

struct Node;

struct NodeList;
typedef struct NodeList NodeList;

static inline void NodeList_init(NodeList* this);
static inline void NodeList_uninit(NodeList* this);
static inline void NodeList_append(NodeList* this, struct Node* node);
static inline void NodeList_removeHead(NodeList* this);
static inline size_t NodeList_length(NodeList* this);

struct NodeList
{
   struct PointerList pointerList;
};


void NodeList_init(NodeList* this)
{
   PointerList_init( (PointerList*)this);
}

void NodeList_uninit(NodeList* this)
{
   PointerList_uninit( (PointerList*)this);
}

void NodeList_append(NodeList* this, struct Node* node)
{
   PointerList_append( (PointerList*)this, node);
}

void NodeList_removeHead(NodeList* this)
{
   PointerList_removeHead( (PointerList*)this);
}

static inline size_t NodeList_length(NodeList* this)
{
   return PointerList_length( (PointerList*)this);
}

#endif /* NODELIST_H_ */
