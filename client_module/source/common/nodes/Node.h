#ifndef NODE_H_
#define NODE_H_

#include <common/Common.h>
#include <common/nodes/NumNodeID.h>
#include <common/threading/Condition.h>
#include <common/threading/Mutex.h>
#include <common/threading/RWLock.h>
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
   unsigned short portUDP, unsigned short portTCP, NicAddressList* nicList,
   NicAddressList* localRdmaNicList);
extern Node* Node_construct(struct App* app, const char* nodeID, NumNodeID nodeNumID,
   unsigned short portUDP, unsigned short portTCP, NicAddressList* nicList,
   NicAddressList* localRdmaNicList);
extern void Node_uninit(Node* this);
extern void __Node_destruct(Node* this);

extern void Node_updateLastHeartbeatT(Node* this);
extern bool Node_updateInterfaces(Node* this, unsigned short portUDP, unsigned short portTCP,
   NicAddressList* nicList);

/**
 * NodeString is a structure to hold a formatted string representation related to a Node.
 *
 * This structure provides a fixed-size buffer that can store various strings associated with a
 * Node, such as the node's alias or a combination of the alias and node type. It is designed to be
 * used in functions where thread-safe operations are required to copy or format node-related
 * strings into a pre-allocated buffer.
 */
typedef struct NodeString {
   // The mgmtd only allows aliases up to 32 characters with characters limited to "a-zA-Z0-9.-_".
   // It is also used for the AliasWithTypeStr which may be up to 66 bytes: 
   //
   // `<32 CHARACTER ALIAS> [<undefined/invalid>:4294967295]\0`
   //
   // Because it is unlikely we ever encounter an <undefined/invalid> node with an ID that is the
   // uint32 max and a 32 character alias the buffer size is set to 64 bytes which corresponds to
   // the cache line on most architectures. Longer strings will be truncated and end with ellipses.
   char buf[64];
} NodeString;

/**
 * Node_copyAliasWithTypeStr copies the alias, numerical ID (for servers) and the node type in a
 * human-readable string. Intended as a convenient way to get a string identifying a node/client for
 * log messages.
 *
 * @param this is which Node to return an identifying string for.
 * @param outStr is where the string should be copied to.
 *
* IMPORTANT: With the shift from string IDs to aliases in b8.0, aliases may be updated dynamically.
 * Callers MUST use this function to access the alias.
 */
extern void Node_copyAliasWithTypeStr(Node *this, NodeString *outStr);
/**
 * Node_copyAlias gets a copy of the node's alias (formerly known as a string ID).
 *
 * @param this is a pointer to the Node structure with the alias to get.
 * @param outStr is where the alias should be copied to.
 *
 * @return string remains valid until Node_putAlias is called; caller must not free the string.
 * IMPORTANT: With the shift from string IDs to aliases in b8.0, aliases may be updated dynamically.
 * Callers MUST use this function to access the alias.
 */
extern void Node_copyAlias(Node *this, NodeString *outStr);

// static
extern const char* Node_nodeTypeToStr(NodeType nodeType);

// getters & setters

/**
 * Node_setNodeAliasAndType() handles thread safe updates to the alias (formerly node string ID),
 * node type, and nodeAliasWithTypeStr. It blocks until other writers (unlikely) have finished
 * making updates or readers (more likely) have released these fields with the corresponding
 * Node_putX functions.
 *
 * @param this is a pointer to the Node structure with the alias, nodeType, and
 * nodeAliasWithTypeStr.
 * @param alias is the alias to set. Copies the alias and does not take ownership of it. The caller
 * is responsible for freeing the alias when appropriate.
 * @param nodeType the nodeType to set.
 *
 * The alias or nodeType can be respectively NULL or set to NODETYPE_Invalid to only update one
 * field and update the nodeAliasWithTypeStr.
 */
extern bool Node_setNodeAliasAndType(Node* this, const char *alias, NodeType nodeType);
static inline NumNodeID Node_getNumID(Node* this);
static inline void Node_setNumID(Node* this, const NumNodeID numID);
static inline void Node_cloneNicList(Node* this, NicAddressList* nicList);
static inline void Node_updateLocalInterfaces(Node* this, NicAddressList* localNicList,
   NicListCapabilities* localNicCaps, NicAddressList* localRdmaNicList);
static inline NodeConnPool* Node_getConnPool(Node* this);
static inline void Node_setIsActive(Node* this, bool isActive);
static inline bool Node_getIsActive(Node* this);
static inline unsigned short Node_getPortUDP(Node* this);
static inline unsigned short Node_getPortTCP(Node* this);
static inline NodeType Node_getNodeType(Node* this);
static inline const char* Node_getNodeTypeStr(Node* this);

enum NodeType
   {NODETYPE_Invalid = 0, NODETYPE_Meta = 1, NODETYPE_Storage = 2, NODETYPE_Client = 3,
   NODETYPE_Mgmt = 4};


struct Node
{
   NumNodeID numID; // numeric ID, assigned by mgmtd server store (unused for clients)

   // Must be locked before accessing the alias, nodeType, or nodeAliasWithTypeStr.
   RWLock aliasAndTypeMu;
   char* alias; // alias (formerly string ID): initially generated locally on each node but thread safe and able to be updated as of b8.0
   NodeType nodeType; // set by NodeStore::addOrUpdate()
   char* nodeAliasWithTypeStr; // for log messages (initially NULL, initialized when the alias is set/updated)

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

NumNodeID Node_getNumID(Node* this)
{
   return this->numID;
}

void Node_setNumID(Node* this, const NumNodeID numID)
{
   this->numID = numID;
}

/**
 * Retrieve NICs for the node.
 *
 * @param nicList an uninitialized NicAddressList. Caller is responsible for
 *        memory management.
 */
void Node_cloneNicList(Node* this, NicAddressList* nicList)
{
   NodeConnPool_cloneNicList(this->connPool, nicList);
}

/**
 * @param localNicList copied
 * @param localNicCaps copied
 * @param localRdmaNicList copied
 */
void Node_updateLocalInterfaces(Node* this, NicAddressList* localNicList,
   NicListCapabilities* localNicCaps, NicAddressList* localRdmaNicList)
{
   NodeConnPool_updateLocalInterfaces(this->connPool, localNicList, localNicCaps, localRdmaNicList);
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
   NodeType nodeType;
   RWLock_readLock(&this->aliasAndTypeMu);
   nodeType = this->nodeType;
   RWLock_readUnlock(&this->aliasAndTypeMu);
   return nodeType;
}

/**
 * Returns human-readable node type string.
 *
 * @return static string (not alloced => don't free it)
 */
const char* Node_getNodeTypeStr(Node* this)
{
   const char* nodeType;
   RWLock_readLock(&this->aliasAndTypeMu);
   nodeType = Node_nodeTypeToStr(this->nodeType);
   RWLock_readUnlock(&this->aliasAndTypeMu);
   return nodeType;
}

#endif /*NODE_H_*/
