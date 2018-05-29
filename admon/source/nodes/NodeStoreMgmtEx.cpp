#include <common/threading/SafeMutexLock.h>
#include <common/toolkit/Random.h>
#include "NodeStoreMgmtEx.h"


NodeStoreMgmtEx::NodeStoreMgmtEx():
   NodeStoreServers(NODETYPE_Mgmt, false)
{
   // nothing to be done here
}


bool NodeStoreMgmtEx::addOrUpdateNode(std::shared_ptr<MgmtNodeEx> node)
{
   // sanity check: don't allow nodeNumID==0 (only mgmtd allows this)
   bool precheckRes = addOrUpdateNodePrecheck(*node);
   if(!precheckRes)
      return false;

   SafeMutexLock mutexLock(&mutex); // L O C K

   bool retVal = addOrUpdateNodeUnlocked(std::move(node), NULL);

   mutexLock.unlock(); // U N L O C K

   return retVal;
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
   bool precheckRes = addOrUpdateNodePrecheck(*node);
   if(!precheckRes)
      return false;

   SafeMutexLock mutexLock(&mutex); // L O C K

   // check if this is a new node

   auto iter = activeNodes.find(nodeNumID);
   if(iter == activeNodes.end() )
   { // new node
      // new node which will be kept => do the DB operations
      NicAddressList nicList = node->getNicList();

      node = boost::make_unique<MgmtNodeEx>(nodeID, nodeNumID, node->getPortUDP(),
            node->getPortTCP(), nicList);
   }

   bool retVal = addOrUpdateNodeUnlocked(std::move(node), NULL);

   mutexLock.unlock(); // U N L O C K

   return retVal;
}

void NodeStoreMgmtEx::deleteAllNodes()
{
   auto node = referenceFirstNode();

   while(node)
   {
      deleteNode(node->getNumID() );

      node = referenceNextNode(node);
   }
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
