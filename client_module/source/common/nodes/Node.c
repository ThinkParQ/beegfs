#include <os/OsCompat.h>
#include "Node.h"

/**
 * @param portUDP value 0 if undefined
 * @param portTCP value 0 if undefined
 * @param nicList an internal copy will be created
 * @param localRdmaNicList an internal copy will be created
 */
void Node_init(Node* this, struct App* app, const char* alias, NumNodeID nodeNumID,
   unsigned short portUDP, unsigned short portTCP, NicAddressList* nicList,
   NicAddressList* localRdmaNicList)
{
   Mutex_init(&this->mutex);
   Condition_init(&this->changeCond);

   Time_init(&this->lastHeartbeatT);
   this->isActive = false;
   kref_init(&this->references);

   this->numID = nodeNumID;
   RWLock_init(&this->aliasAndTypeMu);
   this->alias = NULL;
   this->nodeAliasWithTypeStr = NULL;
   this->nodeType = NODETYPE_Invalid;
   // We don't know the node type at this stage, it is set later.
   Node_setNodeAliasAndType(this, alias, NODETYPE_Invalid);

   this->portUDP = portUDP;

   this->connPool = NodeConnPool_construct(app, this, portTCP, nicList, localRdmaNicList);
}

/**
 * @param nicList an internal copy will be created
 * @param localRdmaNicList an internal copy will be created
 */
Node* Node_construct(struct App* app, const char* nodeID, NumNodeID nodeNumID,
   unsigned short portUDP, unsigned short portTCP, NicAddressList* nicList,
   NicAddressList* localRdmaNicList)
{
   Node* this = (Node*)os_kmalloc(sizeof(*this) );

   Node_init(this, app, nodeID, nodeNumID, portUDP, portTCP, nicList, localRdmaNicList);

   return this;
}

void Node_uninit(Node* this)
{
   SAFE_DESTRUCT(this->connPool, NodeConnPool_destruct);
   SAFE_KFREE(this->alias);
   SAFE_KFREE(this->nodeAliasWithTypeStr);
   Mutex_uninit(&this->mutex);
}

void __Node_destruct(Node* this)
{
   Node_uninit(this);

   kfree(this);
}

void Node_updateLastHeartbeatT(Node* this)
{
   Mutex_lock(&this->mutex);

   Time_setToNow(&this->lastHeartbeatT);

   Condition_broadcast(&this->changeCond);

   Mutex_unlock(&this->mutex);
}

/**
 * @param portUDP value 0 if undefined
 * @param portTCP value 0 if undefined
 * @return true if a port changed
 */
bool Node_updateInterfaces(Node* this, unsigned short portUDP, unsigned short portTCP,
   NicAddressList* nicList)
{
   bool portChanged = false;

   Mutex_lock(&this->mutex);

   if(portUDP && (portUDP != this->portUDP) )
   {
      this->portUDP = portUDP;
      portChanged = true;
   }

   if(NodeConnPool_updateInterfaces(this->connPool, portTCP, nicList) )
      portChanged = true;

   Mutex_unlock(&this->mutex);

   return portChanged;
}

/**
 * Returns human-readable node type.
 *
 * @return static string (not alloced => don't free it)
 */
const char* Node_nodeTypeToStr(NodeType nodeType)
{
   switch(nodeType)
   {
      case NODETYPE_Invalid:
      {
         return "<undefined/invalid>";
      } break;

      case NODETYPE_Meta:
      {
         return "beegfs-meta";
      } break;

      case NODETYPE_Storage:
      {
         return "beegfs-storage";
      } break;

      case NODETYPE_Client:
      {
         return "beegfs-client";
      } break;

      case NODETYPE_Mgmt:
      {
         return "beegfs-mgmtd";
      } break;

      default:
      {
         return "<unknown>";
      } break;
   }
}

inline void putToNodeString(char *from, NodeString *outStr) {
   int result;
   result = snprintf(outStr->buf, sizeof outStr->buf, "%s", from);
   if (unlikely(result < 0)) {
      snprintf(outStr->buf, sizeof(outStr->buf), "<error determining alias>");
   } else if (unlikely((size_t) result >= sizeof outStr->buf)) {
      memcpy(outStr->buf + sizeof outStr->buf - 4, "...\0", 4);
   }
}

void Node_copyAlias(Node *this, NodeString *outStr) {
   RWLock_readLock(&this->aliasAndTypeMu);
   putToNodeString(this->alias, outStr);
   RWLock_readUnlock(&this->aliasAndTypeMu);
}

void Node_copyAliasWithTypeStr(Node *this, NodeString *outStr) {
   RWLock_readLock(&this->aliasAndTypeMu);
   putToNodeString(this->nodeAliasWithTypeStr, outStr);
   RWLock_readUnlock(&this->aliasAndTypeMu);
}

bool Node_setNodeAliasAndType(Node* this, const char *aliasInput, NodeType nodeTypeInput) {
   char *alias = NULL;
   char *aliasAndTypeStr = NULL;
   bool err = false;

   if (!aliasInput && nodeTypeInput == NODETYPE_Invalid) {
       return true; // Nothing to do, return early.
   }

   if (aliasInput) {
      alias = StringTk_strDup(aliasInput);
      if (!alias) {
         return false;
      }
   }

   RWLock_writeLock(&this->aliasAndTypeMu);
   {
      const char *nextAlias = alias ? alias : this->alias;
      NodeType nextNodeType = nodeTypeInput != NODETYPE_Invalid ? nodeTypeInput : this->nodeType;
      if (nextNodeType == NODETYPE_Client) {
         aliasAndTypeStr = kasprintf(GFP_NOFS, "%s [%s:?]", nextAlias, Node_nodeTypeToStr(nextNodeType));
      }
      else {
         aliasAndTypeStr = kasprintf(GFP_NOFS, "%s [%s:%u]", nextAlias, Node_nodeTypeToStr(nextNodeType), this->numID.value);
      }
      if (!aliasAndTypeStr) {
         err = true;
      }
   }
   if (! err) {
      if (alias) {
         swap(this->alias, alias);
      }
      if (nodeTypeInput != NODETYPE_Invalid) {
          swap(this->nodeType, nodeTypeInput);
      }
      if (aliasAndTypeStr) {
         swap(this->nodeAliasWithTypeStr, aliasAndTypeStr);
      }
   }
   RWLock_writeUnlock(&this->aliasAndTypeMu);

   if (alias) {
      kfree(alias);
   }
   if (aliasAndTypeStr) {
      kfree(aliasAndTypeStr);
   }
   return ! err;
}
