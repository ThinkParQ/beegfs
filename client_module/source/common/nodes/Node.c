#include <os/OsCompat.h>
#include "Node.h"

/**
 * @param portUDP value 0 if undefined
 * @param portTCP value 0 if undefined
 * @param nicList an internal copy will be created
 */
void Node_init(Node* this, struct App* app, const char* nodeID, NumNodeID nodeNumID,
   unsigned short portUDP, unsigned short portTCP, NicAddressList* nicList)
{
   Mutex_init(&this->mutex);
   Condition_init(&this->changeCond);

   Time_init(&this->lastHeartbeatT);
   this->isActive = false;
   kref_init(&this->references);

   this->id = StringTk_strDup(nodeID);
   this->numID = nodeNumID;
   this->nodeType = NODETYPE_Invalid; // set by NodeStore::addOrUpdate()
   this->nodeIDWithTypeStr = NULL; // will by initialized on demand by getNodeIDWithTypeStr()

   this->portUDP = portUDP;

   this->connPool = NodeConnPool_construct(app, this, portTCP, nicList);
}

/**
 * @param nicList an internal copy will be created
 */
Node* Node_construct(struct App* app, const char* nodeID, NumNodeID nodeNumID,
   unsigned short portUDP, unsigned short portTCP, NicAddressList* nicList)
{
   Node* this = (Node*)os_kmalloc(sizeof(*this) );

   Node_init(this, app, nodeID, nodeNumID, portUDP, portTCP, nicList);

   return this;
}

void Node_uninit(Node* this)
{
   SAFE_DESTRUCT(this->connPool, NodeConnPool_destruct);

   SAFE_KFREE(this->nodeIDWithTypeStr);
   SAFE_KFREE(this->id);

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

      case NODETYPE_Helperd:
      {
         return "beegfs-helperd";
      } break;

      default:
      {
         return "<unknown>";
      } break;
   }
}

/**
 * Returns the node type dependent ID (numeric ID for servers and string ID for clients) and the
 * node type in a human-readable string.
 *
 * This is intendened as a convenient way to get a string with node ID and type for log messages.
 *
 * @return string is alloc'ed on demand and remains valid until Node_uninit() or _setNodeType() is
 * called; caller may not free the string.
 */
const char* Node_getNodeIDWithTypeStr(Node* this)
{
   const char* retVal;

   Mutex_lock(&this->mutex); // L O C K

   /* we alloc the string here on demand. it gets freed either by _uninit() or when _setNodeType()
      is called */

   if(!this->nodeIDWithTypeStr)
   { // not initialized yet => alloc and init

      if(this->nodeType == NODETYPE_Client)
         this->nodeIDWithTypeStr = kasprintf(GFP_NOFS, "%s %s",
            Node_getNodeTypeStr(this), this->id);
      else
      {
         const char* nodeID = NumNodeID_str(&this->numID);

         this->nodeIDWithTypeStr = kasprintf(GFP_NOFS, "%s %s [ID: %s]",
            Node_getNodeTypeStr(this), this->id, nodeID);
         kfree(nodeID);
      }
   }

   retVal = this->nodeIDWithTypeStr;

   Mutex_unlock(&this->mutex); // U N L O C K

   return retVal;
}
