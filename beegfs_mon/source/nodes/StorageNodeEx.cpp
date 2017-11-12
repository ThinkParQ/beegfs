#include "StorageNodeEx.h"

StorageNodeEx::StorageNodeEx(std::shared_ptr<Node> receivedNode) :
   Node(receivedNode->getID(), receivedNode->getNumID(),
   receivedNode->getPortUDP(), receivedNode->getPortTCP(),
   receivedNode->getConnPool()->getNicList()),
   isResponding(true)
{}

StorageNodeEx::StorageNodeEx(std::shared_ptr<Node> receivedNode,
      std::shared_ptr<StorageNodeEx> oldNode) :
   Node(receivedNode->getID(), receivedNode->getNumID(),
   receivedNode->getPortUDP(), receivedNode->getPortTCP(),
   receivedNode->getConnPool()->getNicList())
{
   setLastStatRequestTime(oldNode->getLastStatRequestTime());
   setIsResponding(oldNode->getIsResponding());
}