#ifndef NODE_H_
#define NODE_H_

#include <common/Common.h>
#include <common/nodes/NumNodeID.h>
#include <common/threading/Condition.h>
#include <common/threading/Mutex.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/Time.h>
#include <linux/kref.h>
#include "NodeConnPool.h"

// forward declaration
struct App;


enum NodeType;
typedef enum NodeType NodeType;

struct Node;
typedef struct Node Node;


extern void Node_init(Node* this, struct App* app, const char* nodeID, NumNodeID nodeNumID,
   unsigned short portUDP, unsigned short portTCP, NicAddressList* nicList);
extern Node* Node_construct(struct App* app, const char* nodeID, NumNodeID nodeNumID,
   unsigned short portUDP, unsigned short portTCP, NicAddressList* nicList);
extern void Node_uninit(Node* this);
extern void __Node_destruct(Node* this);

extern void Node_updateLastHeartbeatT(Node* this);
extern bool Node_updateInterfaces(Node* this, unsigned short portUDP, unsigned short portTCP,
   NicAddressList* nicList);

extern const char* Node_getNodeIDWithTypeStr(Node* this);

// static
extern const char* Node_nodeTypeToStr(NodeType nodeType);

// getters & setters
static inline char* Node_getID(Node* this);
static inline NumNodeID Node_getNumID(Node* this);
static inline void Node_setNumID(Node* this, const NumNodeID numID);
static inline NicAddressList* Node_getNicList(Node* this);
static inline NodeConnPool* Node_getConnPool(Node* this);
static inline void Node_setIsActive(Node* this, bool isActive);
static inline bool Node_getIsActive(Node* this);
static inline unsigned short Node_getPortUDP(Node* this);
static inline unsigned short Node_getPortTCP(Node* this);
static inline NodeType Node_getNodeType(Node* this);
static inline void Node_setNodeType(Node* this, NodeType nodeType);
static inline const char* Node_getNodeTypeStr(Node* this);

enum NodeType
   {NODETYPE_Invalid = 0, NODETYPE_Meta = 1, NODETYPE_Storage = 2, NODETYPE_Client = 3,
   NODETYPE_Mgmt = 4, NODETYPE_Helperd = 5};


struct Node
{
   char* id; // string ID, generated locally on each node; not thread-safe (not meant to be changed)
   NumNodeID numID; // numeric ID, assigned by mgmtd server store (unused for clients)

   NodeType nodeType; // set by NodeStore::addOrUpdate()
   char* nodeIDWithTypeStr; // for log messages (initially NULL, alloc'ed when needed)

   NodeConnPool* connPool;
   unsigned short portUDP;

   Time lastHeartbeatT; // last heartbeat receive time

   bool isActive; // for internal use by the NodeStore only

   Mutex mutex;
   Condition changeCond; // for last heartbeat time only

   struct kref references;

   /* used by node tree */
   struct {
      struct rb_node rbTreeElement;
   } _nodeTree;
};


static inline Node* Node_get(Node* node)
{
   kref_get(&node->references);
   return node;
}

static inline void __Node_put(struct kref* ref)
{
   __Node_destruct(container_of(ref, Node, references) );
}

static inline int Node_put(Node* node)
{
   return kref_put(&node->references, __Node_put);
}

char* Node_getID(Node* this)
{
   return this->id;
}

NumNodeID Node_getNumID(Node* this)
{
   return this->numID;
}

void Node_setNumID(Node* this, const NumNodeID numID)
{
   this->numID = numID;
}

NicAddressList* Node_getNicList(Node* this)
{
   return NodeConnPool_getNicList(this->connPool);
}

NodeConnPool* Node_getConnPool(Node* this)
{
   return this->connPool;
}

/**
 * Note: For nodes that live inside a NodeStore, this method should only be called by the NodeStore.
 */
void Node_setIsActive(Node* this, bool isActive)
{
   Mutex_lock(&this->mutex);

   this->isActive = isActive;

   Mutex_unlock(&this->mutex);
}

bool Node_getIsActive(Node* this)
{
   bool isActive;

   Mutex_lock(&this->mutex);

   isActive = this->isActive;

   Mutex_unlock(&this->mutex);

   return isActive;
}

unsigned short Node_getPortUDP(Node* this)
{
   unsigned short retVal;

   Mutex_lock(&this->mutex);

   retVal = this->portUDP;

   Mutex_unlock(&this->mutex);

   return retVal;
}

unsigned short Node_getPortTCP(Node* this)
{
   return NodeConnPool_getStreamPort(this->connPool);
}

NodeType Node_getNodeType(Node* this)
{
   return this->nodeType;
}

/**
 * Note: Not thread-safe to avoid unnecessary overhead. We assume this is called only in situations
 * where the object is not used by multiple threads, such as NodeStore::addOrUpdate() or
 * Serialization_deserializeNodeList().
 */
void Node_setNodeType(Node* this, NodeType nodeType)
{
   this->nodeType = nodeType;

   SAFE_KFREE(this->nodeIDWithTypeStr); // will be recreated on demand by _getNodeIDWithTypeStr()
}

/**
 * Returns human-readable node type string.
 *
 * @return static string (not alloced => don't free it)
 */
const char* Node_getNodeTypeStr(Node* this)
{
   return Node_nodeTypeToStr(this->nodeType);
}

#endif /*NODE_H_*/
