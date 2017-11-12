#include <common/net/sock/Socket.h>
#include <common/threading/SafeMutexLock.h>
#include "Node.h"

/**
 * @param nodeNumID value 0 if not yet assigned
 * @param portUDP value 0 if undefined
 * @param portTCP value 0 if undefined
 * @param nicList will be forwarded to the NodeConnPool which creates its own internal copy
 */
Node::Node(std::string nodeID, NumNodeID nodeNumID, unsigned short portUDP, unsigned short portTCP,
   const NicAddressList& nicList)
   : id(nodeID), nodeFeatureFlags(NODE_FEATURES_MAX_INDEX)
{
   //this->id = nodeID; // set in constructor list
   this->numID = nodeNumID;
   this->nodeType = NODETYPE_Invalid; // typically initialized by NodeStore::addOrUpdate()
   this->fhgfsVersion = 0;
   this->portUDP = portUDP;

   this->connPool = new NodeConnPool(*this, portTCP, nicList);
}

/**
 * Constructor for derived classes that provide their own connPool, e.g. LocalNode.
 *
 * Note: derived classes: do not forget to set the connPool!
 *
 * @param portUDP value 0 if undefined
 */
Node::Node(std::string nodeID, NumNodeID nodeNumID, unsigned short portUDP)
   : id(nodeID), nodeFeatureFlags(NODE_FEATURES_MAX_INDEX)
{
   //this->id = nodeID; // set in constructor list
   this->numID = nodeNumID;
   this->nodeType = NODETYPE_Invalid; // typically initialized by NodeStore::addOrUpdate()
   this->fhgfsVersion = 0;
   this->portUDP = portUDP;

   // derived classes: do not forget to set the connPool!
}


Node::~Node()
{
   SAFE_DELETE_NOSET(connPool);
}


/**
 * Updates the last heartbeat time.
 */
void Node::updateLastHeartbeatT()
{
   SafeMutexLock mutexLock(&mutex);

   updateLastHeartbeatTUnlocked();

   mutexLock.unlock();
}

/**
 * Updates the last heartbeat time.
 *
 * Note: Does not lock the node mutex, but broadcasts the change condition
 * => make sure the mutex is locked when calling this!!
 */
void Node::updateLastHeartbeatTUnlocked()
{
   lastHeartbeatT.setToNow();

   changeCond.broadcast();
}

/**
 * Gets the last heartbeat time.
 */
Time Node::getLastHeartbeatT()
{
   SafeMutexLock mutexLock(&mutex);

   Time t(lastHeartbeatT);

   mutexLock.unlock();

   return t;
}

/**
 * Gets the last heartbeat time.
 *
 *  Note: Does not lock the node mutex
 * => make sure the mutex is locked when calling this!
 */
Time Node::getLastHeartbeatTUnlocked()
{
   Time t(lastHeartbeatT);
   return t;
}

/**
 * Waits for a heartbeat time update.
 *
 * @return true if the lastHeartbeatTime changed, false on timeout
 */
bool Node::waitForNewHeartbeatT(Time* oldT, int timeoutMS)
{
   bool heartbeatChanged;
   int remainingTimeoutMS = timeoutMS;

   Time startT;

   SafeMutexLock mutexLock(&mutex);


   while( (remainingTimeoutMS > 0) && (*oldT == lastHeartbeatT) )
   {
      if(!changeCond.timedwait(&mutex, remainingTimeoutMS) )
         break; // timeout

      remainingTimeoutMS = timeoutMS - startT.elapsedMS();
   }

   heartbeatChanged = (*oldT != lastHeartbeatT);


   mutexLock.unlock();


   return heartbeatChanged;

}

/**
 * @param portUDP value 0 if undefined
 * @param portTCP value 0 if undefined
 * @param nicList will be copied
 */
void Node::updateInterfaces(unsigned short portUDP, unsigned short portTCP, NicAddressList& nicList)
{
   SafeMutexLock mutexLock(&mutex);

   updateInterfacesUnlocked(portUDP, portTCP, nicList);

   mutexLock.unlock();
}

/**
 * @param portUDP value 0 if undefined
 * @param portTCP value 0 if undefined
 * @param nicList will be copied
 */
void Node::updateInterfacesUnlocked(unsigned short portUDP, unsigned short portTCP,
   NicAddressList& nicList)
{
   this->portUDP = portUDP ? portUDP : this->portUDP;

   this->connPool->updateInterfaces(portTCP, nicList);
}

/**
 * Convenience-wrapper for the static version of this method.
 */
std::string Node::getTypedNodeID() const
{
   return getTypedNodeID(id, numID, nodeType);
}

std::string Node::getTypedNodeID(std::string nodeID, NumNodeID nodeNumID, NodeType nodeType)
{
   return nodeID + " [ID: " + nodeNumID.str() + "]";
}

/**
 * Convenience-wrapper for the static version of this method.
 */
std::string Node::getNodeIDWithTypeStr() const
{
   return getNodeIDWithTypeStr(id, numID, nodeType);
}

/**
 * Returns the node type dependent ID (numeric ID for servers and string ID for clients) and the
 * node type in a human-readable string.
 *
 * This is intendened as a convenient way to get a string with node ID and type for log messages.
 */
std::string Node::getNodeIDWithTypeStr(std::string nodeID, NumNodeID nodeNumID, NodeType nodeType)
{
   return nodeTypeToStr(nodeType) + " " + getTypedNodeID(nodeID, nodeNumID, nodeType);
}

/**
 * Convenience-wrapper for the static version of this method.
 */
std::string Node::getNodeTypeStr() const
{
   return nodeTypeToStr(nodeType);
}

/**
 * Returns human-readable node type.
 */
std::string Node::nodeTypeToStr(NodeType nodeType)
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

      case NODETYPE_Admon:
      {
         return "beegfs-admon";
      } break;

      case NODETYPE_CacheDaemon:
      {
         return "beegfs-cached";
      } break;

      case NODETYPE_CacheLib:
      {
         return "beegfs-cache-lib";
      } break;

      default:
      {
         return "<unknown(" + StringTk::intToStr(nodeType) + ")>";
      } break;
   }
}

/**
 * Note: not thread-safe, so use this only when there are no other threads accessing this node
 * object.
 *
 * @param featureFlags will be copied.
 */
void Node::setFeatureFlags(const BitStore* featureFlags)
{
   SafeMutexLock lock(&mutex);

   this->nodeFeatureFlags = *featureFlags;

   lock.unlock();
}
