#include "MetaNodeEx.h"

MetaNodeEx::MetaNodeEx(std::shared_ptr<Node> receivedNode) :
   Node(NODETYPE_Meta, receivedNode->getID(), receivedNode->getNumID(),
   receivedNode->getPortUDP(), receivedNode->getPortTCP(),
   receivedNode->getConnPool()->getNicList()),
   isResponding(true)
{}

MetaNodeEx::MetaNodeEx(std::shared_ptr<Node> receivedNode, std::shared_ptr<MetaNodeEx> oldNode) :
   Node(NODETYPE_Meta, receivedNode->getID(), receivedNode->getNumID(),
   receivedNode->getPortUDP(), receivedNode->getPortTCP(),
   receivedNode->getConnPool()->getNicList())
{
   setLastStatRequestTime(oldNode->getLastStatRequestTime());
   setIsResponding(oldNode->getIsResponding());
}
