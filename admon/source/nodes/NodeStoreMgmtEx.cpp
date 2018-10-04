#include "NodeStoreMgmtEx.h"

#include <mutex>

NodeStoreMgmtEx::NodeStoreMgmtEx():
   NodeStoreServers(NODETYPE_Mgmt, false)
{
   // nothing to be done here
}


bool NodeStoreMgmtEx::addOrUpdateNode(std::shared_ptr<MgmtNodeEx> node)
{
   // sanity check: don't allow nodeNumID==0 (only mgmtd allows this)
   if (!node->getNumID())
      return false;

   const std::lock_guard<Mutex> lock(mutex);

   return addOrUpdateNodeUnlocked(std::move(node), NULL);
}

/**
 * this function should only be called by sync nodes, all others use the typed version.
 *
 * it is important here that we only insert the specialized node objects into the store, not the
 * generic "class Node" objects.
 */
bool NodeStoreMgmtEx::addOrUpdateNode(std::shared_ptr<Node> node)
{
   LogContext context("add/update node");
   context.logErr("Wrong method used!");
   context.logBacktrace();

   return false;
}

bool NodeStoreMgmtEx::addOrUpdateNodeEx(std::shared_ptr<Node> node, NumNodeID* outNodeNumID)
{
   std::string nodeID(node->getID());
   NumNodeID nodeNumID = node->getNumID();

   // sanity check: don't allow nodeNumID==0 (only mgmtd allows this)
   if (!node->getNumID())
      return false;

   const std::lock_guard<Mutex> lock(mutex);

   // check if this is a new node

   auto iter = activeNodes.find(nodeNumID);
   if(iter == activeNodes.end() )
   { // new node
      // new node which will be kept => do the DB operations
      NicAddressList nicList = node->getNicList();

      node = boost::make_unique<MgmtNodeEx>(nodeID, nodeNumID, node->getPortUDP(),
            node->getPortTCP(), nicList);
   }

   return addOrUpdateNodeUnlocked(std::move(node), NULL);
}

void NodeStoreMgmtEx::deleteAllNodes()
{
   for (const auto& node : referenceAllNodes())
      deleteNode(node->getNumID() );
}

bool NodeStoreMgmtEx::statusMgmtd(NumNodeID nodeID, std::string *outInfo)
{
   bool res = true;
   *outInfo = "Up";
   auto node = std::static_pointer_cast<MgmtNodeEx>(referenceNode(nodeID));

   if (node != NULL)
   {
      if (!node->getContent().isResponding)
      {
         res = false;
         *outInfo = "Down";
      }
   }
   else
   {
      res = false;
      *outInfo = "Node not available!";
   }

   return res;
}
