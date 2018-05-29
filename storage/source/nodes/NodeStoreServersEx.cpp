#include "NodeStoreServersEx.h"


NodeStoreServersEx::NodeStoreServersEx(NodeType storeType) :
   NodeStoreServers(storeType, true)
{
   // nothing to be done here (all done in initializer list)
}

bool NodeStoreServersEx::addOrUpdateNode(std::shared_ptr<Node> node)
{
   // sanity check: don't allow nodeNumID==0 (only mgmtd allows this)
   bool precheckRes = addOrUpdateNodePrecheck(*node);
   if(!precheckRes)
      return false; // precheck automatically deletes *node and sets it to NULL


   return NodeStoreServers::addOrUpdateNode(std::move(node));
}
