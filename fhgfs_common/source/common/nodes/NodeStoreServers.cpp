#include <common/app/log/LogContext.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/threading/SafeMutexLock.h>
#include "NodeStoreServers.h"

#include <limits.h>
#include <mutex>


 /*
  * maximum nuber of IDs for server nodes, -1 for reserved value "0"
  * note: at the moment we restrict server node IDs to 16 bit to be compatible with targetIDs
  */
#define NODESTORESERVERS_MAX_NODENUMIDS (USHRT_MAX-1)

/**
 * Note: Does not initialize the localNode data (the localNode can be set later)
 *
 * @param storeType will be applied to contained nodes on addOrUpdate()
 * @param channelsDirectDefault false to make all channels indirect by default (only metadata
 *    server should set this to true, all others to false)
 */
NodeStoreServers::NodeStoreServers(NodeType storeType, bool channelsDirectDefault)
   throw(InvalidConfigException) : AbstractNodeStore(storeType)
{
   this->channelsDirectDefault = channelsDirectDefault;
   this->capacityPools = NULL;
   this->targetMapper = NULL;
   this->stateStore = NULL;

   this->rootIsBuddyMirrored = 0;
}

/**
 * Just forwards to the other addOrUpdateNodeEx method (to provide compatibility with the
 * corresponding virtual AbstractNodeStore::addOrUpdateNode() method).
 */
bool NodeStoreServers::addOrUpdateNode(NodeHandle node)
{
   return addOrUpdateNodeEx(std::move(node), NULL);
}

/**
 * Locking wrapper for addOrUpdateNodeUnlocked().
 * See addOrUpdateNodeUnlocked() for description.
 */
bool NodeStoreServers::addOrUpdateNodeEx(NodeHandle node, NumNodeID* outNodeNumID)
{
   std::lock_guard<Mutex> lock(mutex);
   return addOrUpdateNodeUnlocked(std::move(node), outNodeNumID);
}

/**
 * Note: unlocked, so caller must hold lock.
 *
 * @param node belongs to the store after calling this method; this method will set (*node=NULL);
 * so do not free it and don't use it any more afterwards (reference it from this store if you need
 * it)
 * @param outNodeNumID will be set to the the node's numeric ID if no error occurred, 0 otherwise
 * (may be NULL if caller doesn't care).
 * @return true if the node was not in the active group yet and has been added to the active
 * group (i.e. new node), false otherwise
 */
bool NodeStoreServers::addOrUpdateNodeUnlocked(NodeHandle node, NumNodeID* outNodeNumID)
{
   NumNodeID nodeNumID = node->getNumID();
   std::string nodeID(node->getID());

   // check if given node is localNode

   if(localNode && (nodeNumID == localNode->getNumID() ) )
   { // we don't allow to update local node this way (would lead to problems during sync etc.)
      SAFE_ASSIGN(outNodeNumID, nodeNumID);
      return false;
   }

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


   // check if this is a new node or an update of an existing node

   /* note: the activeNodes.find(nodeNumID) below needs to be done even if node->getNumID() was
      initially 0, because generateNodeNumID() might have found that the node already had a numID
      and just didn't know about it (e.g. when it sent a registration retry message). */

   bool nodeWasInserted = false; // true if node was inserted as new node into active store

   auto iter = activeNodes.find(nodeNumID);

   if (iter != activeNodes.end())
   { // node was in the active store already => update it
      Node& active = *iter->second;
      NicAddressList nicList(node->getNicList());

      if (unlikely(active.getID() != node->getID()))
      { // bad: numeric ID collision for two different node string IDs
         LogContext(__func__).logErr(
            std::string("Numeric ID collision for two different string IDs: ") +
            nodeID + " / " + active.getID() );

         nodeNumID = NumNodeID(); // set to invalid ID, so caller hopefully checks outNodeNumID
      }
      else
      { // update node
         active.updateLastHeartbeatT();
         active.setFhgfsVersion(node->getFhgfsVersion());
         active.updateInterfaces(node->getPortUDP(), node->getPortTCP(), nicList);
         active.setFeatureFlags(node->getNodeFeatures());
      }

      SAFE_ASSIGN(outNodeNumID, nodeNumID);
   }
   else
   { // node wasn't in active store yet

      // make sure we don't have this stringID with a different numID already
      NumNodeID existingNumID = retrieveNumIDFromStringID(node->getID());
      if(existingNumID && existingNumID != nodeNumID)
      { // reject node (stringID was already registered with a different numID)
         SAFE_ASSIGN(outNodeNumID, existingNumID);
      }
      else
      { // no conflicts detected => insert new node

         #ifdef BEEGFS_DEBUG
         // check whether node type and store type differ
         if (node->getNodeType() != NODETYPE_Invalid && node->getNodeType() != storeType)
         {
            LogContext(__func__).logErr("Node type and store type differ. "
               "Node: " + node->getNodeIDWithTypeStr() + "; "
               "Store: " + Node::nodeTypeToStr(storeType) );
         }
         #endif // BEEGFS_DEBUG

         node->setNodeType(storeType);
         node->getConnPool()->setChannelDirect(channelsDirectDefault);

         handleNodeVersion(*node);

         activeNodes.insert({nodeNumID, std::move(node)});

         newNodeCond.broadcast();

         SAFE_ASSIGN(outNodeNumID, nodeNumID);

         nodeWasInserted = true;
      }
   }

   return nodeWasInserted;
}

/**
 * Checks whether the given node's numeric ID is !=0.
 * This is a sanity check that should be used by all except mgmtd in their addOrUpdateNode()
 * methods. It will also print a log message when the numeric ID is 0.
 *
 * Note: This method only accesses no internal data structures, so no locking is required.
 *
 * @node if the numeric ID is 0, *node will be deleted and the pointer will be set to NULL.
 * @return false if numeric ID was 0.
 */
bool NodeStoreServers::addOrUpdateNodePrecheck(Node& node)
{
   NumNodeID nodeNumID = node.getNumID();

   // sanity check: don't allow nodeNumID==0 (only mgmtd allows this)
   if(unlikely(!nodeNumID) )
   { // numeric ID not set - should never happen
      LogContext(__func__).logErr("Should never happen: Rejecting nodeNumID==0. "
         "NodeID: " + node.getID() + "; "
         "Type: " + node.getNodeTypeStr() );

      return false;
   }

   return true;
}

bool NodeStoreServers::updateLastHeartbeatT(NumNodeID id)
{
   auto node = referenceNode(id);
   if(!node)
      return false;

   node->updateLastHeartbeatT();

   return true;
}


/**
 * @return NULL if no such node exists
 */
NodeHandle NodeStoreServers::referenceNode(NumNodeID id)
{
   // check for invalid id 0
   #ifdef BEEGFS_DEBUG
   if(!id)
      LogContext(__func__).log(Log_CRITICAL, "BUG?: Attempt to reference numeric node ID '0'");
   #endif // BEEGFS_DEBUG

   std::lock_guard<Mutex> lock(mutex);

   if (localNode && id == localNode->getNumID())
      return localNode;

   // search in active nodes map
   auto iter = activeNodes.find(id);
   if (iter != activeNodes.end())
      return iter->second;

   return {};
}

/**
 * @return NULL if no such node exists
 */
NodeHandle NodeStoreServers::referenceRootNode(MirrorBuddyGroupMapper* metaBuddyGroupMapper)
{
   std::lock_guard<Mutex> lock(mutex);

   const NumNodeID nodeNumID = rootIsBuddyMirrored
      ? NumNodeID(metaBuddyGroupMapper->getPrimaryTargetID(rootNodeID.val()))
      : rootNodeID;

   if (localNode && nodeNumID == localNode->getNumID())
      return localNode;

   // search in active nodes map
   auto iter = activeNodes.find(nodeNumID);
   if (iter != activeNodes.end())
      return iter->second;

   return {};
}

/**
 * This is used to iterate over all stored nodes.
 * Start with this and then use referenceNextNode() until it returns NULL.
 *
 * @return can be NULL if localNode is not included
 */
NodeHandle NodeStoreServers::referenceFirstNode()
{
   std::lock_guard<Mutex> lock(mutex);

   if (activeNodes.empty())
      return localNode;

   const auto firstNodeInMap = activeNodes.cbegin();

   if (localNode && localNode->getNumID() < firstNodeInMap->first)
      return localNode;
   else
      return firstNodeInMap->second;
}

/**
 * References the next node.
 *
 * @return NULL if oldNode was the last node
 */
NodeHandle NodeStoreServers::referenceNextNode(const NodeHandle& oldNode)
{
   std::lock_guard<Mutex> lock(mutex);

   auto id = oldNode->getNumID();

   auto nextNodeInMap = activeNodes.upper_bound(id);

   if (nextNodeInMap == activeNodes.end())
   {
      if (localNode && id < localNode->getNumID())
         return localNode;
      else
         return {};
   }
   else
   {
      if (localNode && id < localNode->getNumID() && localNode->getNumID() < nextNodeInMap->first)
         return localNode;
      else
         return nextNodeInMap->second;
   }
}

/**
 * This is a helper to have only one call for the typical targetMapper->getNodeID() and following
 * referenceNode() calls.
 *
 * @param targetMapper where to resolve the given targetID
 * @param outErr will be set to FhgfsOpsErr_UNKNOWNNODE, _UNKNOWNTARGET, _SUCCESS (may be NULL)
 * @return NULL if targetID is not mapped or if the mapped node does not exist in the store.
 */
NodeHandle NodeStoreServers::referenceNodeByTargetID(uint16_t targetID, TargetMapper* targetMapper,
   FhgfsOpsErr* outErr)
{
   NumNodeID nodeID = targetMapper->getNodeID(targetID);
   if(unlikely(!nodeID) )
   { // no such target mapping exists
      SAFE_ASSIGN(outErr, FhgfsOpsErr_UNKNOWNTARGET);
      return {};
   }

   auto node = referenceNode(nodeID);
   if (unlikely(!node))
   { // no node with this id exists
      SAFE_ASSIGN(outErr, FhgfsOpsErr_UNKNOWNNODE);
      return {};
   }

   SAFE_ASSIGN(outErr, FhgfsOpsErr_SUCCESS);
   return node;
}

/**
 * Note: Very inefficient (needs to walk over all nodes in store), so only use this if performance
 * is not critical.
 *
 * @return NULL if node was not found.
 */
NodeHandle NodeStoreServers::referenceNodeByStringID(std::string nodeStrID)
{
   auto node = referenceFirstNode();
   while(node)
   {
      if(node->getID() == nodeStrID)
         return node;

      node = referenceNextNode(node);
   }

   return node;
}

/**
 * @return true if node was active before deletion
 */
bool NodeStoreServers::deleteNode(NumNodeID id)
{
   std::lock_guard<Mutex> lock(mutex);

   auto erased = activeNodes.erase(id);
   if (erased > 0)
   {
      // forward removal to (optionally) attached objects

      if(capacityPools)
         capacityPools->remove(id.val()); // remove node from capacity pools

      if(targetMapper)
         targetMapper->unmapByNodeID(id); // remove node from target mapper

      if(stateStore)
         stateStore->removeTarget(id.val()); // remove node from target states

      return true;
   }

   return false;
}


std::vector<NodeHandle> NodeStoreServers::referenceAllNodes()
{
   std::lock_guard<Mutex> lock(mutex);

   // ensure that localNode sits at the appropriate place in outList. that place will be before
   // the first node from activeNodes that has a greater ID than localNode, and after the last
   // node in activeNodes that has a smaller ID.
   // since activeNodes is sorted by ID, this can be done by seeding outList with localNode and
   // adding new nodes either before or after the last element, depending on the ID ordering.

   std::vector<NodeHandle> result;

   result.reserve(activeNodes.size() + 1);

   if (localNode)
      result.push_back(localNode);

   for (auto iter = activeNodes.begin(); iter != activeNodes.end(); iter++)
   {
      if (result.empty() || result.back()->getNumID() < iter->first)
         result.insert(result.end(), iter->second);
      else
         result.insert(std::prev(result.end()), iter->second);
   }

   return result;
}

/**
 * Check whether a node exists in the (active) store.
 *
 * @return true if the node exists
 */
bool NodeStoreServers::isNodeActive(NumNodeID id)
{
   std::lock_guard<Mutex> lock(mutex);

   if (localNode && id == localNode->getNumID())
      return true;

   return activeNodes.find(id) != activeNodes.end();
}

/**
 * Get number of (active) nodes in store.
 *
 * note: returned size includes the localnode
 */
size_t NodeStoreServers::getSize()
{
   std::lock_guard<Mutex> lock(mutex);
   return localNode ? (activeNodes.size() + 1) : activeNodes.size();
}

NumNodeID NodeStoreServers::getRootNodeNumID()
{
   std::lock_guard<Mutex> lock(mutex);
   return rootNodeID;
}

bool NodeStoreServers::getRootIsBuddyMirrored()
{
   std::lock_guard<Mutex> lock(mutex);
   return rootIsBuddyMirrored;
}


/**
 * Set internal root node ID.
 *
 * @return false if the new ID was rejected (e.g. because we already had an id set and
 * ignoreExistingRoot was false).
 */
bool NodeStoreServers::setRootNodeNumID(NumNodeID id, bool ignoreExistingRoot, bool isBuddyMirrored)
{
   // don't allow invalid id 0 (if not forced to do so)
   if(!id && !ignoreExistingRoot)
      return false;

   std::lock_guard<Mutex> lock(mutex);

   bool setRootRes = true;

   if(!this->rootNodeID)
   { // no root defined yet => set the new root
      this->rootNodeID = id;
      // set buddy mirrored info
      rootIsBuddyMirrored = isBuddyMirrored;
   }
   else
   if(!ignoreExistingRoot)
   { // root defined already, reject new root
      setRootRes = false;
   }
   else
   { // root defined already, but shall be ignored
      this->rootNodeID = id;
      // set buddy mirrored info
      rootIsBuddyMirrored = isBuddyMirrored;
   }

   return setRootRes;
}

/**
 * Waits for the first node that is added to the store.
 *
 * @return true if a new node was added to the store before the timeout expired
 */
bool NodeStoreServers::waitForFirstNode(int waitTimeoutMS)
{
   int elapsedMS;

   std::lock_guard<Mutex> lock(mutex);

   Time startT;
   elapsedMS = 0;

   while(!activeNodes.size() && (elapsedMS < waitTimeoutMS) )
   {
      newNodeCond.timedwait(&mutex, waitTimeoutMS - elapsedMS);

      elapsedMS = startT.elapsedMS();
   }

   return activeNodes.size() > 0;
}

/**
 * @param masterList must be ordered; contained nodes will be removed and may no longer be
 * accessed after calling this method.
 * @param updateExisting true to call addOrUpdate for nodes that already existed in the store
 * @param appLocalNode just what you get from app->getLocalNode(), to determine NIC capabilities
 */
void NodeStoreServers::syncNodes(const std::vector<NodeHandle>& masterList,
   NumNodeIDList* outAddedNumIDs, NumNodeIDList* outRemovedNumIDs, bool updateExisting,
   Node* appLocalNode)
{
   // Note: We have two phases here:
   //    Phase 1 (locked): Identify added/removed nodes.
   //    Phase 2 (unlocked): Add/remove nodes from store.
   // This separation is required to not break compatibility with virtual overwritten add/remove
   // methods in derived classes (e.g. fhgfs_mgmtd).


   // P H A S E 1 (Identify added/removed nodes.)

   std::unique_lock<Mutex> lock(mutex);

   const NumNodeID localNodeNumID(localNode ? localNode->getNumID() : NumNodeID(0));
   std::vector<NodeHandle> addLaterNodes; // nodes to be added in phase 2

   // temporary insertion of localNode
   if (localNode)
      activeNodes.insert({localNodeNumID, localNode});

   auto activeIter = activeNodes.begin();
   auto masterIter = masterList.begin();

   while (activeIter != activeNodes.end() && masterIter != masterList.end())
   {
      NumNodeID currentActive = activeIter->first;
      NumNodeID currentMaster = (*masterIter)->getNumID();

      if(currentMaster < currentActive)
      { // currentMaster is added
         outAddedNumIDs->push_back(currentMaster);
         addLaterNodes.push_back(*masterIter);

         masterIter++;
      }
      else
      if(currentActive < currentMaster)
      { // currentActive is removed (but don't ever remove localNode)
         if(likely(localNodeNumID != currentActive) )
            outRemovedNumIDs->push_back(currentActive);

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
      outAddedNumIDs->push_back( (*masterIter)->getNumID() );
      addLaterNodes.push_back(*masterIter);
      masterIter++;
   }

   // remaining active nodes are removed
   for( ; activeIter != activeNodes.end(); activeIter++)
   {
      if(likely(localNodeNumID != activeIter->first) ) // (don't ever remove localNode)
         outRemovedNumIDs->push_back(activeIter->first);
   }


   if(localNode) // remove temporarily inserted localNode
      activeNodes.erase(localNodeNumID);

   lock.unlock();


   // P H A S E 2 (Add/remove nodes from store.)

   // remove nodes
   NumNodeIDListIter iter = outRemovedNumIDs->begin();
   while(iter != outRemovedNumIDs->end() )
   {
      NumNodeID nodeID = *iter;
      iter++; // (removal invalidates iter)

      deleteNode(nodeID);
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

      addOrUpdateNodeEx(node, NULL);
   }
}

/**
 * Nodes will automatically be added/removed from attached capacity pools when they are
 * added/removed from this store.
 */
void NodeStoreServers::attachCapacityPools(NodeCapacityPools* capacityPools)
{
   std::lock_guard<Mutex> lock(mutex);
   this->capacityPools = capacityPools;
}

/**
 * Nodes will automatically be removed from attached target mapper when they are
 * removed from this store.
 */
void NodeStoreServers::attachTargetMapper(TargetMapper* targetMapper)
{
   std::lock_guard<Mutex> lock(mutex);
   this->targetMapper = targetMapper;
}

/**
 * Nodes will automatically be removed from attached state store when they are
 * removed from this store.
 */
void NodeStoreServers::attachStateStore(TargetStateStore* stateStore)
{
   std::lock_guard<Mutex> lock(mutex);
   this->stateStore = stateStore;
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
NumNodeID NodeStoreServers::generateNodeNumID(Node& node)
{
   // check whether this node's stringID is already associated with an active or deleted numID

   NumNodeID previousNumID = retrieveNumIDFromStringID(node.getID() );
   if(previousNumID)
      return previousNumID;

   // check if we have a numeric ID left

   size_t numNodesTotal = activeNodes.size();

   // note: at the moment we restrict server node IDs to 16 bit to be compatible with targetIDs
   if(unlikely(numNodesTotal >= NODESTORESERVERS_MAX_NODENUMIDS) )
      return NumNodeID(); // all numeric IDs currently in use


   /* walk from 1 to find the first free ID.
      (this could take quite some time if almost all numeric IDs are in use, but we assume that case
      is rare and we don't care too much about performance here.) */

   for(uint32_t newNumID = 1; newNumID != 0; newNumID++)
   {
      NumNodeID newID(newNumID);
      if(!checkNodeNumIDCollision(newID) )
         return newID; // we found an ID that no other node uses
   }

   return NumNodeID();
}

/**
 * Search activeNodes for a node with the given string ID and return it's associated numeric ID.
 *
 * Note: Caller must hold lock.
 *
 * @return 0 if no node with the given stringID was found, associated numeric ID otherwise.
 */
NumNodeID NodeStoreServers::retrieveNumIDFromStringID(std::string nodeID)
{
   if(localNode && (nodeID == localNode->getID() ) )
      return localNode->getNumID();

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
bool NodeStoreServers::checkNodeNumIDCollision(NumNodeID numID)
{
   if(localNode && (numID == localNode->getNumID() ) )
      return true;

   auto activeIter = activeNodes.find(numID);
   if (activeIter != activeNodes.end())
      return true; // we found a node with the given numID

   return false; // no node uses the given numID
}

/**
 * Take special actions based on version of a (typically new) node, e.g. compat flags deactivation.
 *
 * Note: Caller must hold lock.
 */
void NodeStoreServers::handleNodeVersion(Node& node)
{
   // nothing to be done here currently
}

/**
 * Get complete node IDs and type for log messages from numerical ID.
 *
 * Note: This method is expensive, so use this function only in performance uncritical code paths.
 *       If you already have a Node object, call node->getNodeIDWithTypeStr() directly.
 */
std::string NodeStoreServers::getNodeIDWithTypeStr(NumNodeID numID)
{
   std::lock_guard<Mutex> lock(mutex);

   auto iter = activeNodes.find(numID);
   if (iter != activeNodes.end())
      return iter->second->getNodeIDWithTypeStr();

   if (localNode && numID == localNode->getNumID())
      return localNode->getNodeIDWithTypeStr();

   return "Unknown node: " + Node::nodeTypeToStr(storeType) + " [ID: " + numID.str() + "]";
}

/**
 * Get complete node IDs for log messages from numerical ID.
 *
 * Note: This method is expensive, so use this function only in performance uncritical code paths.
 *       If you already have a Node object, call node->getTypedNodeID() directly.
 */
std::string NodeStoreServers::getTypedNodeID(NumNodeID numID)
{
   std::lock_guard<Mutex> lock(mutex);

   auto iter = activeNodes.find(numID);
   if (iter != activeNodes.end() )
      return iter->second->getTypedNodeID();

   if (localNode && numID == localNode->getNumID())
      return localNode->getTypedNodeID();

   return "Unknown node: " + Node::nodeTypeToStr(storeType) + " [ID: " + numID.str() + "]";
}
