#include <common/net/sock/Socket.h>
#include "Node.h"

#include <mutex>

#include <boost/format.hpp>

/**
 * @param nodeNumID value 0 if not yet assigned
 * @param portUDP value 0 if undefined
 * @param portTCP value 0 if undefined
 * @param nicList will be forwarded to the NodeConnPool which creates its own internal copy
 */
Node::Node(NodeType nodeType, std::string nodeID, NumNodeID nodeNumID, unsigned short portUDP,
      unsigned short portTCP, const NicAddressList& nicList) :
   nodeType(nodeType), id(nodeID)
{
   this->numID = nodeNumID;
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
Node::Node(NodeType nodeType, std::string nodeID, NumNodeID nodeNumID, unsigned short portUDP):
   nodeType(nodeType), id(std::move(nodeID))
{
   this->numID = nodeNumID;
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
   const std::lock_guard<Mutex> lock(mutex);

   updateLastHeartbeatTUnlocked();
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
}

/**
 * Gets the last heartbeat time.
 */
Time Node::getLastHeartbeatT()
{
   const std::lock_guard<Mutex> lock(mutex);

   return lastHeartbeatT;
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
 * @param portUDP value 0 if undefined
 * @param portTCP value 0 if undefined
 * @param nicList will be copied
 */
bool Node::updateInterfaces(unsigned short portUDP, unsigned short portTCP, NicAddressList& nicList)
{
   const std::lock_guard<Mutex> lock(mutex);

   return updateInterfacesUnlocked(portUDP, portTCP, nicList);
}

/**
 * @param portUDP value 0 if undefined
 * @param portTCP value 0 if undefined
 * @param nicList will be copied
 */
bool Node::updateInterfacesUnlocked(unsigned short portUDP, unsigned short portTCP,
   NicAddressList& nicList)
{
   this->portUDP = portUDP ? portUDP : this->portUDP;

   return this->connPool->updateInterfaces(portTCP, nicList);
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
   return str(boost::format("%1% %2%") % nodeType
            % getTypedNodeID(std::move(nodeID), nodeNumID, nodeType));
}
