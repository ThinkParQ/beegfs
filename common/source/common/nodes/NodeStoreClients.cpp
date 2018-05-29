#include <common/threading/SafeMutexLock.h>
#include <common/app/log/LogContext.h>
#include "NodeStoreClients.h"

#include <limits.h>
#include <mutex>

/* maximum number of IDs for client nodes, -1 for reserved value "0" */
#define NODESTORESCLIENTS_MAX_NODENUMIDS (UINT_MAX-1)

/**
 * Note: Does not initialize the localNode data (the localNode can be set later)
 *
 * @param channelsDirectDefault false to make all channels indirect by default (only metadata
 *    server should set this to true, all others to false)
 */
NodeStoreClients::NodeStoreClients(bool channelsDirectDefault) :
   AbstractNodeStore(NODETYPE_Client)
{
   this->channelsDirectDefault = channelsDirectDefault;
}

/**
 * Just forwards to the other addOrUpdateNodeEx method (to provide compatibility with the
 * corresponding virtual AbstractNodeStore::addOrUpdateNode() method).
 */
bool NodeStoreClients::addOrUpdateNode(NodeHandle node)
{
   return addOrUpdateNodeEx(std::move(node), NULL);
}

/**
 * @param node belongs to the store after calling this method; this method will set (*node=NULL);
 * so do not free it and don't use it any more afterwards (reference it from this store if you need
 * it)
 * @return true if the node was not in the active group yet, false otherwise
 */
bool NodeStoreClients::addOrUpdateNodeEx(NodeHandle node, NumNodeID* outNodeNumID)
{
   NumNodeID nodeNumID = node->getNumID();
   std::string nodeID = node->getID();

   std::unique_lock<Mutex> lock(mutex);

   // check if numID is given

   if(!nodeNumID)
   { // no numID yet => generate it
      nodeNumID = generateNodeNumID(*node);
      if(unlikely(!nodeNumID) )
      { // no numID available
         LogContext(__func__).logErr(
            std::string("Unable to add new node, ran out of numeric IDs. NodeID: ") + nodeID);

         SAFE_ASSIGN(outNodeNumID, nodeNumID);
         return false;
      }

      node->setNumID(nodeNumID); // assign new numeric ID
   }

   // is node in any of the stores already?

   const auto activeIter = activeNodes.find(nodeNumID);
   const bool nodeWasActive = activeIter != activeNodes.end();
   if (nodeWasActive)
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
         active.setFhgfsVersion(node->getFhgfsVersion());
         active.updateInterfaces(node->getPortUDP(), node->getPortTCP(), nicList);
         active.setFeatureFlags(node->getNodeFeatures());
      }

      SAFE_ASSIGN(outNodeNumID, nodeNumID);
   }
   else
   { // node not in any store yet
      node->getConnPool()->setChannelDirect(channelsDirectDefault);

      activeNodes.insert({nodeNumID, std::move(node)});

      newNodeCond.broadcast();

      SAFE_ASSIGN(outNodeNumID, nodeNumID);
   }

   lock.unlock();

   // set lastUsedNumID if this ID is bigger than lastUsedNumID
   if (nodeNumID > lastUsedNumID)
      lastUsedNumID = nodeNumID;

   return !nodeWasActive;
}

bool NodeStoreClients::updateLastHeartbeatT(NumNodeID numNodeID)
{
   auto node = referenceNode(numNodeID);
   if (!node)
      return false;

   node->updateLastHeartbeatT();
   return true;
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
 * @return NULL if nodeNumID was the last node
 */
NodeHandle NodeStoreClients::referenceNextNode(const NodeHandle& oldNode) const
{
   std::lock_guard<Mutex> lock(mutex);

   auto nextNodeInMap = activeNodes.upper_bound(oldNode->getNumID());
   if (nextNodeInMap == activeNodes.end())
      return {};
   else
      return nextNodeInMap->second;
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
 * @param updateExisting true to call addOrUpdate for nodes that already existed in the store
 * @param appLocalNode (just what you get from app->getLocalNode() )
 */
void NodeStoreClients::syncNodes(const std::vector<NodeHandle>& masterList,
   NumNodeIDList* outAddedIDs, NumNodeIDList* outRemovedIDs, bool updateExisting,
   Node* appLocalNode)
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
         if (updateExisting)
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


   // set supported nic capabilities for added nodes
   NicListCapabilities localNicCaps;

   if(appLocalNode)
   {
      NicAddressList localNicList(appLocalNode->getNicList() );
      NetworkInterfaceCard::supportedCapabilities(&localNicList, &localNicCaps);
   }

   // add nodes
   for (auto iter = addLaterNodes.begin(); iter != addLaterNodes.end(); iter++)
   {
      auto& node = *iter;

      if(appLocalNode)
         node->getConnPool()->setLocalNicCaps(&localNicCaps);

      addOrUpdateNode(node);
   }
}

/**
 * Generate a new numeric node ID and assign it to the node.
 * This method will also first check whether this node already has a numeric ID assigned and just
 * didn't know of it yet (e.g. because of thread races.)
 *
 * Note: Caller must hold lock.
 * Note: Caller is expected to add the node to the activeNodes map and assign the numeric ID
 *       (because new IDs are picked based on assigned IDs in the activeNodes map).
 *
 * @return 0 if all available numIDs are currently assigned, so none are left
 */
NumNodeID NodeStoreClients::generateNodeNumID(Node& node)
{
   // check whether this node's stringID is already associated with an active or deleted numID
   NumNodeID previousNumID = retrieveNumIDFromStringID(node.getID() );
   if(previousNumID)
      return previousNumID;

   // check if we have a numeric ID left
   size_t numNodesTotal = activeNodes.size();

   //note: at the moment we restrict server node IDs to 16 bit to be compatible with targetIDs,
   //client node IDs can be 32 bit
   if(unlikely(numNodesTotal >= NODESTORESCLIENTS_MAX_NODENUMIDS) )
      return NumNodeID(); // all numeric IDs currently in use

   // the following code will most likely prevent numeric IDs for clients from collisions, which
   // can occur if a client gets unmounted and its ID gets re-used before all nodes in the system
   // know about the client removal. However, in case we have a lot of clients (really a lot, close
   // to NODESTORESCLIENTS_MAX_NODENUMIDS), collisions can still happen

   // as long as we haven't reachend NODESTORESCLIENTS_MAX_NODENUMIDS (which is pretty unlikely),
   // this should return the direct successor of lastUsedID.
   // if NODESTORESCLIENTS_MAX_NODENUMIDS is already reached, we have to find a spot in between
   for(uint32_t nextID = lastUsedNumID.val() + 1;
         nextID <= NODESTORESCLIENTS_MAX_NODENUMIDS;
         nextID++)
   {
      if(!checkNodeNumIDCollision(NumNodeID(nextID) ) )
         return NumNodeID(nextID); // we found an ID that no other node uses
   }

   // if we came here, we couldn't find a free spot between lastUsedID and
   // NODESTORESCLIENTS_MAX_NODENUMIDS. In this case we have to try beginning with 1
   for(uint32_t newNumID = 1; newNumID <= lastUsedNumID.val(); newNumID++)
   {
      if(!checkNodeNumIDCollision(NumNodeID(newNumID) ) )
      {
         lastUsedNumID = NumNodeID(newNumID); // in this case we have to set lastUsedNumID here
         return lastUsedNumID; // we found an ID that no other node uses
      }
   }

   // still no new ID => all IDs used!

   return NumNodeID();
}

/**
 * Search activeNodes for a node with the given string ID and return it's associated numeric ID.
 *
 * Note: Caller must hold lock.
 *
 * @return 0 if no node with the given stringID was found, associated numeric ID otherwise.
 */
NumNodeID NodeStoreClients::retrieveNumIDFromStringID(std::string nodeID) const
{
   for (auto iter = activeNodes.begin(); iter != activeNodes.end(); iter++)
   {
      Node& currentNode = *iter->second;

      if (currentNode.getID() == nodeID)
         return currentNode.getNumID();
   }

   return NumNodeID();
}

/**
 * Check whether the given numeric ID is already being used.
 *
 * Note: Caller must hold lock.
 *
 * @return true if any of the active or deleted nodes already uses the given numeric ID, false
 * otherwise.
 */
bool NodeStoreClients::checkNodeNumIDCollision(NumNodeID numID) const
{
   auto activeIter = activeNodes.find(numID);
   if (activeIter != activeNodes.end() )
      return true; // we found a node with the given numID

   return false; // no node uses the given numID
}
