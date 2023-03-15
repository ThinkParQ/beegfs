#include "NodeStoreMgmtEx.h"

NodeStoreMgmtEx::NodeStoreMgmtEx() :
      NodeStoreServers(NODETYPE_Mgmt, false)
{}

NodeStoreResult NodeStoreMgmtEx::addOrUpdateNodeEx(std::shared_ptr<Node> node, NumNodeID* outNodeNumID)
{
   std::string nodeID(node->getID());
   NumNodeID nodeNumID = node->getNumID();

   // sanity check: don't allow nodeNumID==0 (only mgmtd allows this)
   if (!node->getNumID())
      return NodeStoreResult::Error;

   const std::lock_guard<Mutex> lock(mutex);

   // check if this is a new node
   auto iter = activeNodes.find(nodeNumID);
   if (iter == activeNodes.end() )
   {
      NicAddressList nicList = node->getNicList();

      node = boost::make_unique<MgmtNodeEx>(nodeID, nodeNumID, node->getPortUDP(),
            node->getPortTCP(), nicList);
   }

   return addOrUpdateNodeUnlocked(std::move(node), outNodeNumID);
}
