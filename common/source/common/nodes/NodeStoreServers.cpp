#include <common/app/log/LogContext.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include "NodeStoreServers.h"

#include <climits>
#include <mutex>

#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>


/**
 * Note: Does not initialize the localNode data (the localNode can be set later)
 *
 * @param storeType will be applied to contained nodes on addOrUpdate()
 * @param channelsDirectDefault false to make all channels indirect by default (only metadata
 *    server should set this to true, all others to false)
 */
NodeStoreServers::NodeStoreServers(NodeType storeType, bool channelsDirectDefault)
   : AbstractNodeStore(storeType)
{
   this->channelsDirectDefault = channelsDirectDefault;
   this->capacityPools = NULL;
   this->targetMapper = NULL;
   this->stateStore = NULL;
}

/**
 * Just forwards to the other addOrUpdateNodeEx method (to provide compatibility with the
 * corresponding virtual AbstractNodeStore::addOrUpdateNode() method).
 */
NodeStoreResult NodeStoreServers::addOrUpdateNode(NodeHandle node)
{
   return addOrUpdateNodeEx(std::move(node), NULL);
}

/**
 * Locking wrapper for addOrUpdateNodeUnlocked().
 * See addOrUpdateNodeUnlocked() for description.
 */
NodeStoreResult NodeStoreServers::addOrUpdateNodeEx(NodeHandle node, NumNodeID* outNodeNumID)
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
NodeStoreResult NodeStoreServers::addOrUpdateNodeUnlocked(NodeHandle node, NumNodeID* outNodeNumID)
{
   NumNodeID nodeNumID = node->getNumID();
   std::string nodeID(node->getID());

   // check if given node is localNode

   if(localNode && (nodeNumID == localNode->getNumID() ) )
   { // we don't allow to update local node this way (would lead to problems during sync etc.)
      SAFE_ASSIGN(outNodeNumID, nodeNumID);
      return NodeStoreResult::Error;
   }

   // check if numID is given. if not, we must generate an id - only mgmt may do that, which it
   // should do in a subclass.
   if (!nodeNumID)
   {
      auto newID = generateID(*node);
      if (!newID)
      {
         SAFE_ASSIGN(outNodeNumID, NumNodeID());
         LOG(GENERAL, ERR, "Should never happen: Rejecting nodeNumID==0.",
               node->getNodeIDWithTypeStr());
         return NodeStoreResult::Error;
      }

      nodeNumID = newID;
      node->setNumID(nodeNumID);
   }


   // check if this is a new node or an update of an existing node

   /* note: the activeNodes.find(nodeNumID) below needs to be done even if node->getNumID() was
      initially 0, because generateNodeNumID() might have found that the node already had a numID
      and just didn't know about it (e.g. when it sent a registration retry message). */

   NodeStoreResult result = NodeStoreResult::Unchanged;

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
         if (active.updateInterfaces(node->getPortUDP(), node->getPortTCP(), nicList))
         {
            result = NodeStoreResult::Updated;
         }
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

         // check whether node type and store type differ
         if (node->getNodeType() != NODETYPE_Invalid && node->getNodeType() != storeType)
         {
            LogContext(__func__).logErr("Node type and store type differ. "
               "Node: " + node->getNodeIDWithTypeStr() + "; "
               "Store: " + boost::lexical_cast<std::string>(storeType));
            throw std::runtime_error("inserted wrong node type into store");
         }

         node->getConnPool()->setChannelDirect(channelsDirectDefault);

         activeNodes.insert({nodeNumID, std::move(node)});

         newNodeCond.broadcast();

         SAFE_ASSIGN(outNodeNumID, nodeNumID);

         result = NodeStoreResult::Added;
      }
   }

   return result;
}

/**
 * @return NULL if no such node exists
 */
NodeHandle NodeStoreServers::referenceNode(NumNodeID id) const
{
   // check for invalid id 0
   #ifdef BEEGFS_DEBUG
   if(!id)
      LogContext(__func__).log(Log_CRITICAL, "BUG?: Attempt to reference numeric node ID '0'");
   #endif // BEEGFS_DEBUG

   std::lock_guard<Mutex> lock(mutex);

   auto iter = activeNodes.find(id);
   if (iter != activeNodes.end())
      return iter->second;

   return {};
}

/**
 * @return can be NULL if localNode is not included
 */
NodeHandle NodeStoreServers::referenceFirstNode() const
{
   std::lock_guard<Mutex> lock(mutex);

   if (!activeNodes.empty())
      return activeNodes.cbegin()->second;

   return {};
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
   FhgfsOpsErr* outErr) const
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
 * @return true if node was active before deletion
 */
bool NodeStoreServers::deleteNode(NumNodeID id)
{
   std::lock_guard<Mutex> lock(mutex);

   if (localNode && localNode->getNumID() == id)
      return false;

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


std::vector<NodeHandle> NodeStoreServers::referenceAllNodes() const
{
   std::lock_guard<Mutex> lock(mutex);

   std::vector<NodeHandle> result;

   result.reserve(activeNodes.size());

   for (auto iter = activeNodes.begin(); iter != activeNodes.end(); iter++)
      result.push_back(iter->second);

   return result;
}

/**
 * Check whether a node exists in the (active) store.
 *
 * @return true if the node exists
 */
bool NodeStoreServers::isNodeActive(NumNodeID id) const
{
   std::lock_guard<Mutex> lock(mutex);

   return activeNodes.find(id) != activeNodes.end();
}

/**
 * Get number of (active) nodes in store.
 *
 * note: returned size includes the localnode
 */
size_t NodeStoreServers::getSize() const
{
   std::lock_guard<Mutex> lock(mutex);
   return activeNodes.size();
}

/**
 * Waits for the first node that is added to the store.
 *
 * @return true if a new node was added to the store before the timeout expired
 */
bool NodeStoreServers::waitForFirstNode(int waitTimeoutMS) const
{
   int elapsedMS;

   std::lock_guard<Mutex> lock(mutex);

   Time startT;
   elapsedMS = 0;

   while (activeNodes.empty() && (elapsedMS < waitTimeoutMS))
   {
      newNodeCond.timedwait(&mutex, waitTimeoutMS - elapsedMS);

      elapsedMS = startT.elapsedMS();
   }

   return !activeNodes.empty();
}

/**
 * @param masterList must be ordered; contained nodes will be removed and may no longer be
 * accessed after calling this method.
 * @param appLocalNode just what you get from app->getLocalNode(), to determine NIC capabilities
 */
void NodeStoreServers::syncNodes(const std::vector<NodeHandle>& masterList,
   NumNodeIDList* outAddedNumIDs, NumNodeIDList* outRemovedNumIDs, Node* appLocalNode)
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
   NicAddressList localNicList;

   if(appLocalNode)
   {
      localNicList = appLocalNode->getNicList();
      NetworkInterfaceCard::supportedCapabilities(&localNicList, &localNicCaps);
   }

   // add nodes
   for (auto iter = addLaterNodes.begin(); iter != addLaterNodes.end(); iter++)
   {
      auto& node = *iter;

      if(appLocalNode)
         node->getConnPool()->setLocalNicList(localNicList, localNicCaps);

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
 * Search activeNodes for a node with the given string ID and return it's associated numeric ID.
 *
 * Note: Caller must hold lock.
 *
 * @return 0 if no node with the given stringID was found, associated numeric ID otherwise.
 */
NumNodeID NodeStoreServers::retrieveNumIDFromStringID(std::string nodeID) const
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
 * Get complete node IDs and type for log messages from numerical ID.
 *
 * Note: This method is expensive, so use this function only in performance uncritical code paths.
 *       If you already have a Node object, call node->getNodeIDWithTypeStr() directly.
 */
std::string NodeStoreServers::getNodeIDWithTypeStr(NumNodeID numID) const
{
   std::lock_guard<Mutex> lock(mutex);

   auto iter = activeNodes.find(numID);
   if (iter != activeNodes.end())
      return iter->second->getNodeIDWithTypeStr();

   return str(boost::format("Unknown node: %1% [ID: %2%]") % storeType % numID);
}

/**
 * Get complete node IDs for log messages from numerical ID.
 *
 * Note: This method is expensive, so use this function only in performance uncritical code paths.
 *       If you already have a Node object, call node->getTypedNodeID() directly.
 */
std::string NodeStoreServers::getTypedNodeID(NumNodeID numID) const
{
   std::lock_guard<Mutex> lock(mutex);

   auto iter = activeNodes.find(numID);
   if (iter != activeNodes.end() )
      return iter->second->getTypedNodeID();

   return str(boost::format("Unknown node: %1% [ID: %2%]") % storeType % numID);
}
