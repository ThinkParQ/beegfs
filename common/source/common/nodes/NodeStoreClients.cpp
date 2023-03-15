#include <common/app/log/LogContext.h>
#include "NodeStoreClients.h"

#include <climits>
#include <mutex>

/**
 * Note: Does not initialize the localNode data (the localNode can be set later)
 *
 * @param channelsDirectDefault false to make all channels indirect by default (only metadata
 *    server should set this to true, all others to false)
 */
NodeStoreClients::NodeStoreClients():
   AbstractNodeStore(NODETYPE_Client)
{
}

/**
 * Just forwards to the other addOrUpdateNodeEx method (to provide compatibility with the
 * corresponding virtual AbstractNodeStore::addOrUpdateNode() method).
 */
NodeStoreResult NodeStoreClients::addOrUpdateNode(NodeHandle node)
{
   return addOrUpdateNodeEx(std::move(node), NULL);
}

/**
 * @param node belongs to the store after calling this method; this method will set (*node=NULL);
 * so do not free it and don't use it any more afterwards (reference it from this store if you need
 * it)
 * @return true if the node was not in the active group yet, false otherwise
 */
NodeStoreResult NodeStoreClients::addOrUpdateNodeEx(NodeHandle node, NumNodeID* outNodeNumID)
{
   NumNodeID nodeNumID = node->getNumID();
   std::string nodeID = node->getID();

   const std::lock_guard<Mutex> lock(mutex);

   // check if numID is given. if not, we must generate an id - only mgmt may do that, which it
   // should do in a subclass.
   if (!nodeNumID)
   {
      auto newID = generateID(*node);
      if (!newID)
         return NodeStoreResult::Error;

      nodeNumID = newID;
      node->setNumID(nodeNumID);
   }

   // is node in any of the stores already?

   const auto activeIter = activeNodes.find(nodeNumID);
   NodeStoreResult result = NodeStoreResult::Unchanged;
   if (activeIter != activeNodes.end())
   { // node was in the store already => update it
      Node& active = *activeIter->second;
      NicAddressList nicList(node->getNicList());

      if (unlikely(active.getID() != nodeID))
      { // bad: numeric ID collision for two different node string IDs
         LogContext(__func__).logErr(
            std::string("Numeric ID collision for two different string IDs: ") + nodeID + " / "
               + active.getID());

         nodeNumID = NumNodeID(); // set to invalid ID, so caller hopefully checks outNodeNumID
      }
      else
      {
         active.updateLastHeartbeatT();
         if (active.updateInterfaces(node->getPortUDP(), node->getPortTCP(), nicList))
            result = NodeStoreResult::Updated;
      }

      SAFE_ASSIGN(outNodeNumID, nodeNumID);
   }
   else
   { // node not in any store yet
      node->getConnPool()->setChannelDirect(false);

      activeNodes.insert({nodeNumID, std::move(node)});

      newNodeCond.broadcast();

      SAFE_ASSIGN(outNodeNumID, nodeNumID);

      result = NodeStoreResult::Added;
   }

   return result;
}


/**
 * @return NULL if no such node exists
 */
NodeHandle NodeStoreClients::referenceNode(NumNodeID numNodeID) const
{
   std::lock_guard<Mutex> lock(mutex);

   auto iter = activeNodes.find(numNodeID);
   if (iter != activeNodes.end())
      return iter->second;

   return {};
}

/**
 * @return can be NULL
 */
NodeHandle NodeStoreClients::referenceFirstNode() const
{
   std::lock_guard<Mutex> lock(mutex);

   auto iter = activeNodes.begin();
   if (iter != activeNodes.end())
      return iter->second;

   return {};
}

/**
 * @return true if node was active before deletion
 */
bool NodeStoreClients::deleteNode(NumNodeID nodeNumID)
{
   std::lock_guard<Mutex> lock(mutex);

   auto erased = activeNodes.erase(nodeNumID);
   return erased > 0;
}

std::vector<NodeHandle> NodeStoreClients::referenceAllNodes() const
{
   std::lock_guard<Mutex> lock(mutex);

   std::vector<NodeHandle> result;

   result.reserve(activeNodes.size());

   for (auto iter = activeNodes.begin(); iter != activeNodes.end(); iter++)
      result.push_back(iter->second);

   return result;
}

bool NodeStoreClients::isNodeActive(NumNodeID nodeNumID) const
{
   std::lock_guard<Mutex> lock(mutex);

   return activeNodes.find(nodeNumID) != activeNodes.end();
}

/**
 * Get number of (active) nodes in store.
 *
 * note: returned size includes the localnode
 */
size_t NodeStoreClients::getSize() const
{
   std::lock_guard<Mutex> lock(mutex);
   return activeNodes.size();
}

/**
 * @param masterList must be ordered; contained nodes will be removed and may no longer be
 * accessed after calling this method.
 * @param appLocalNode (just what you get from app->getLocalNode() )
 */
void NodeStoreClients::syncNodes(const std::vector<NodeHandle>& masterList,
   NumNodeIDList* outAddedIDs, NumNodeIDList* outRemovedIDs)
{
   // Note: We have two phases here:
   //    Phase 1 (locked): Identify added/removed nodes.
   //    Phase 2 (unlocked): Add/remove nodes from store.
   // This separation is required to not break compatibility with virtual overwritten add/remove
   // methods in derived classes (e.g. fhgfs_mgmtd).


   // P H A S E 1 (Identify added/removed nodes.)

   std::string localNodeID;
   std::vector<NodeHandle> addLaterNodes; // nodes to be added in phase 2

   std::unique_lock<Mutex> lock(mutex);

   auto activeIter = activeNodes.begin();
   auto masterIter = masterList.begin();

   while (activeIter != activeNodes.end() && masterIter != masterList.end())
   {
      NumNodeID currentActive = activeIter->first;
      NumNodeID currentMaster = (*masterIter)->getNumID();

      if(currentMaster < currentActive)
      { // currentMaster is added
         outAddedIDs->push_back(currentMaster);
         addLaterNodes.push_back(*masterIter);

         masterIter++;
      }
      else
      if(currentActive < currentMaster)
      { // currentActive is removed
         outRemovedIDs->push_back(currentActive);

         activeIter++;
      }
      else
      { // node unchanged
         addLaterNodes.push_back(*masterIter);

         masterIter++;
         activeIter++;
      }
   }

   // remaining masterList nodes are added
   while (masterIter != masterList.end())
   {
      outAddedIDs->push_back( (*masterIter)->getNumID() );
      addLaterNodes.push_back(*masterIter);

      masterIter++;
   }

   // remaining active nodes are removed
   for ( ; activeIter != activeNodes.end(); activeIter++ )
      outRemovedIDs->push_back(activeIter->first);

   lock.unlock();


   // P H A S E 2 (Add/remove nodes from store.)

   // remove nodes
   NumNodeIDListIter iter = outRemovedIDs->begin();
   while(iter != outRemovedIDs->end() )
   {
      NumNodeID nodeNumID = *iter;
      iter++; // (removal invalidates iter)

      deleteNode(nodeNumID);
   }


   // add nodes
   for (auto iter = addLaterNodes.begin(); iter != addLaterNodes.end(); iter++)
   {
      auto& node = *iter;

      addOrUpdateNode(node);
   }
}
