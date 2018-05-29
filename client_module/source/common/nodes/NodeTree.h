#ifndef NodeTree_h_cfEUS0RiTiwS20ayW10wHn
#define NodeTree_h_cfEUS0RiTiwS20ayW10wHn

#include <common/nodes/Node.h>
#include <linux/rbtree.h>


struct NodeTree;
typedef struct NodeTree NodeTree;

struct NodeTreeIter;
typedef struct NodeTreeIter NodeTreeIter;



extern void NodeTree_init(NodeTree* this);
extern void NodeTree_uninit(NodeTree* this);

extern Node* NodeTree_find(NodeTree* this, NumNodeID nodeNumID);
extern Node* NodeTree_getNext(NodeTree* this, Node* node);

extern bool NodeTree_insert(NodeTree* this, NumNodeID nodeNumID, Node* node);
extern bool NodeTree_erase(NodeTree* this, NumNodeID nodeNumID);


static inline void NodeTreeIter_init(NodeTreeIter* this, NodeTree* tree);
static inline void NodeTreeIter_next(NodeTreeIter* this);
static inline Node* NodeTreeIter_value(NodeTreeIter* this);
static inline bool NodeTreeIter_end(NodeTreeIter* this);

struct NodeTree
{
   struct rb_root nodes;
   unsigned size;
};

struct NodeTreeIter
{
   struct rb_node* value;
};



void NodeTreeIter_init(NodeTreeIter* this, NodeTree* tree)
{
   this->value = rb_first(&tree->nodes);
}

void NodeTreeIter_next(NodeTreeIter* this)
{
   this->value = rb_next(this->value);
}

Node* NodeTreeIter_value(NodeTreeIter* this)
{
   return rb_entry(this->value, Node, _nodeTree.rbTreeElement);
}

bool NodeTreeIter_end(NodeTreeIter* this)
{
   return this->value == NULL;
}

#endif
