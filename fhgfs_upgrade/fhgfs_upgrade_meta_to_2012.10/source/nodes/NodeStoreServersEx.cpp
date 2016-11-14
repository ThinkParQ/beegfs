#include <common/threading/SafeMutexLock.h>
#include "NodeStoreServersEx.h"


NodeStoreServersEx::NodeStoreServersEx(NodeType storeType) throw(InvalidConfigException) :
   NodeStoreServers(storeType, true)
{
   // nothing to be done here (all done in initializer list)
}

bool NodeStoreServersEx::addOrUpdateNode(Node** node)
{
   // sanity check: don't allow nodeNumID==0 (only mgmtd allows this)
   bool precheckRes = addOrUpdateNodePrecheck(node);
   if(!precheckRes)
      return false; // precheck automatically deletes *node and sets it to NULL


   return NodeStoreServers::addOrUpdateNode(node);
}


/**
 * Note: Only works when localNode is included.
 */
bool NodeStoreServersEx::gotRoot()
{
   SafeMutexLock mutexLock(&mutex); // L O C K

   bool gotRootRes = (rootNodeID == localNode->getNumID() );

   mutexLock.unlock(); // U N L O C K

   return gotRootRes;
}



