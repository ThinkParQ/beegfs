#include <common/threading/SafeMutexLock.h>
#include <common/app/log/LogContext.h>
#include "NodeStoreServers.h"

#include <limits.h>


#define NODESTORE_MAX_NODENUMIDS (USHRT_MAX-1) /* -1 for reserved value "0" */


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
   this->localNode = NULL;
   this->channelsDirectDefault = channelsDirectDefault;
   this->capacityPools = NULL;
   this->targetMapper = NULL;

   this->rootNodeID = 0;
}


NodeStoreServers::~NodeStoreServers()
{
   for(NodeMapIter iter = activeNodes.begin(); iter != activeNodes.end(); iter++)
      delete(iter->second);

   activeNodes.clear();

   for(NodePtrMapIter iter = deletedNodes.begin(); iter != deletedNodes.end(); iter++)
      delete(iter->second);

   deletedNodes.clear();
}

/**
 * Just forwards to the other addOrUpdateNode method (to provide compatibility with the
 * corresponding virtual AbstractNodeStore::addOrUpdateNode() method).
 */
bool NodeStoreServers::addOrUpdateNode(Node** node)
{
   return addOrUpdateNode(node, NULL);
}

/**
 * Locking wrapper for addOrUpdateNodeUnlocked().
 * See addOrUpdateNodeUnlocked() for description.
 */
bool NodeStoreServers::addOrUpdateNode(Node** node, uint16_t* outNodeNumID)
{
   SafeMutexLock mutexLock(&mutex); // L O C K

   bool retVal = addOrUpdateNodeUnlocked(node, outNodeNumID);

   mutexLock.unlock(); // U N L O C K

   return retVal;
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
bool NodeStoreServers::addOrUpdateNodeUnlocked(Node** node, uint16_t* outNodeNumID)
{
   uint16_t nodeNumID = (*node)->getNumID();
   std::string nodeID( (*node)->getID() );

   // check if given node is localNode

   if(localNode && (nodeNumID == localNode->getNumID() ) )
   { // we don't allow to update local node this way (would lead to problems during sync etc.)
      delete(*node);
      *node = NULL;

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

         delete(*node);
         *node = NULL;

         SAFE_ASSIGN(outNodeNumID, nodeNumID);
         return false;
      }

      (*node)->setNumID(nodeNumID); // assign new numeric ID
   }


   // check if this is a new node or an update of an existing node

   /* note: the activeNodes.find(nodeNumID) below needs to be done even if node->getNumID() was
      initially 0, because generateNodeNumID() might have found that the node already had a numID
      and just didn't know about it (e.g. when it sent a registration retry message). */

   NodeMapIter iter = activeNodes.find(nodeNumID);
   bool nodeWasActive = (iter != activeNodes.end() );

   if(nodeWasActive)
   { // node was in the active store already => update it
      NodeReferencer* nodeRefer = iter->second;
      Node* nodeNonRef = nodeRefer->getReferencedObject();
      NicAddressList nicList( (*node)->getNicList() );

      if(unlikely(nodeNonRef->getID() != (*node)->getID() ) )
      { // bad: numeric ID collision for two different node string IDs
         LogContext(__func__).logErr(
            std::string("Numeric ID collision for two different string IDs: ") +
            nodeID + " / " + nodeNonRef->getID() );

         nodeNumID = 0; // set to invalid ID, so caller hopefully checks outNodeNumID
      }
      else
      { // update node
         nodeNonRef->updateLastHeartbeatT();
         nodeNonRef->setFhgfsVersion( (*node)->getFhgfsVersion() );
         nodeNonRef->updateInterfaces( (*node)->getPortUDP(), (*node)->getPortTCP(), nicList);
      }

      delete(*node);
   }
   else
   { // node wasn't in active store yet => insert it
      activeNodes.insert(NodeMapVal(nodeNumID, new NodeReferencer(*node) ) );

      #ifdef FHGFS_DEBUG
      // check whether node type and store type differ
      if( ( (*node)->getNodeType() != NODETYPE_Invalid) &&
          ( (*node)->getNodeType() != storeType) )
      {
         LogContext(__func__).logErr("Node type and store type differ. "
            "Node: " + (*node)->getNodeIDWithTypeStr() + "; "
            "Store: " + Node::nodeTypeToStr(storeType) );
      }
      #endif // FHGFS_DEBUG

      (*node)->setNodeType(storeType);
      (*node)->getConnPool()->setChannelDirect(channelsDirectDefault);

      newNodeCond.broadcast();
   }


   *node = NULL;
   SAFE_ASSIGN(outNodeNumID, nodeNumID);

   return !nodeWasActive;
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
bool NodeStoreServers::addOrUpdateNodePrecheck(Node** node)
{
   uint16_t nodeNumID = (*node)->getNumID();

   // sanity check: don't allow nodeNumID==0 (only mgmtd allows this)
   if(unlikely(!nodeNumID) )
   { // numeric ID not set - should never happen
      LogContext(__func__).logErr("Should never happen: Rejecting nodeNumID==0. "
         "NodeID: " + (*node)->getID() + "; "
         "Type: " + (*node)->getNodeTypeStr() );

      delete(*node);
      *node = NULL;

      return false;
   }

   return true;
}

bool NodeStoreServers::updateLastHeartbeatT(uint16_t id)
{
   Node* node = referenceNode(id);
   if(!node)
      return false;

   node->updateLastHeartbeatT();

   releaseNode(&node);

   return true;
}


/**
 * Note: remember to call releaseNode()
 *
 * @return NULL if no such node exists
 */
Node* NodeStoreServers::referenceNode(uint16_t id)
{
   // check for invalid id 0
   #ifdef FHGFS_DEBUG
   if(!id)
      LogContext(__func__).log(Log_CRITICAL, "BUG?: Attempt to reference numeric node ID '0'");
   #endif // FHGFS_DEBUG

   SafeMutexLock mutexLock(&mutex); // L O C K

   Node* node = NULL;

   if(localNode && (id == localNode->getNumID() ) )
      node = localNode;
   else
   { // search in active nodes map

      NodeMapIter iter = activeNodes.find(id);
      if(iter != activeNodes.end() )
      { // found it
         NodeReferencer* nodeRefer = iter->second;
         node = nodeRefer->reference();

         // check for unusually high reference count
         #ifdef FHGFS_DEBUG
         if(nodeRefer->getRefCount() > 100)
            LogContext(__func__).log(Log_CRITICAL,
               std::string("WARNING: Lots of references to node (=> leak?): ") +
               node->getID() + " ref count: " + StringTk::intToStr(nodeRefer->getRefCount() ) );
         #endif // FHGFS_DEBUG
      }
   }

   mutexLock.unlock(); // U N L O C K

   return node;
}

/**
 * Note: remember to call releaseNode()
 *
 * @return NULL if no such node exists
 */
Node* NodeStoreServers::referenceRootNode()
{
   SafeMutexLock mutexLock(&mutex); // L O C K

   Node* node = NULL;

   if(localNode && (rootNodeID == localNode->getNumID() ) )
      node = localNode;
   else
   { // search in active nodes map

      NodeMapIter iter = activeNodes.find(rootNodeID);
      if(iter != activeNodes.end() )
      { // found it
         NodeReferencer* nodeRefer = iter->second;
         node = nodeRefer->reference();
      }
   }

   mutexLock.unlock(); // U N L O C K

   return node;
}

/**
 * Note: remember to call releaseNode()
 *
 * @return can be NULL if localNode is not included
 */
Node* NodeStoreServers::referenceRandomNode()
{
   SafeMutexLock mutexLock(&mutex); // L O C K

   Node* resultNode = NULL;

   NodeReferencer localNodeRefer(localNode, false); /* don't move this into if-brackets, because it
      would be removed from stack then and we need to access it again further below.*/

   if(localNode) // insert local node
      activeNodes.insert(NodeMapVal(localNode->getNumID(), &localNodeRefer) );

   NodeMapIter iter = activeNodes.begin();

   moveIterToRandomElem<NodeMap, NodeMapIter>(activeNodes, iter);

   if(iter != activeNodes.end() )
   {
      NodeReferencer* nodeRefer = iter->second;
      resultNode = nodeRefer->reference();
   }

   if(localNode) // remove local node
      activeNodes.erase(localNode->getNumID() );

   mutexLock.unlock(); // U N L O C K

   return resultNode;
}


/**
 * This is used to iterate over all stored nodes.
 * Start with this and then use referenceNextNode() until it returns NULL.
 *
 * Note: remember to call releaseNode()
 *
 * @return can be NULL if localNode is not included
 */
Node* NodeStoreServers::referenceFirstNode()
{
   SafeMutexLock mutexLock(&mutex); // L O C K

   Node* resultNode = NULL;

   NodeReferencer localNodeRefer(localNode, false); /* don't move this into if-brackets, because it
      would be removed from stack then and we need to access it again further below.*/

   if(localNode) // insert local node
      activeNodes.insert(NodeMapVal(localNode->getNumID(), &localNodeRefer) );

   NodeMapIter iter = activeNodes.begin();

   if(iter != activeNodes.end() )
   {
      NodeReferencer* nodeRefer = iter->second;
      resultNode = nodeRefer->reference();
   }

   if(localNode) // remove local node
      activeNodes.erase(localNode->getNumID() );

   mutexLock.unlock(); // U N L O C K

   return resultNode;
}

/**
 * Note: remember to call releaseNode()
 *
 * @param id the previous node ID, that you got from a call to referenceFirstNode/referenceNextNode
 * @return NULL if nodeID was the last node
 */
Node* NodeStoreServers::referenceNextNode(uint16_t id)
{
   SafeMutexLock mutexLock(&mutex); // L O C K

   Node* resultNode = NULL;
   bool nodeIDexisted = true;

   // find next by inserting localNode and id param (if it didn't exist already) and moving
   // the iterator to the next node after id

   bool insertLocalNode = (localNode && (id <= localNode->getNumID() ) ); /* note: if id
      is already larger than localNodeID, then localNode can't possibly be the next node */

   NodeReferencer localNodeRefer(localNode, false); /* don't move this into if-brackets, because it
      would be removed from stack then and we need to access it again further below.*/

   if(insertLocalNode) // insert local node
      activeNodes.insert(NodeMapVal(localNode->getNumID(), &localNodeRefer) );

   // search for previous node ID
   NodeMapIter iter = activeNodes.find(id);
   if(iter == activeNodes.end() )
   { // ID doesn't exist => insert it
      iter = activeNodes.insert(NodeMapVal(id, NULL) ).first;
      nodeIDexisted = false;
   }

   iter++; // move to next node

   if(iter != activeNodes.end() )
   { // got a next node
      NodeReferencer* nodeRefer = iter->second;
      resultNode = nodeRefer->reference();
   }

   if(insertLocalNode) // remove local node
      activeNodes.erase(localNode->getNumID() );

   if(!nodeIDexisted) // remove inserted previous ID
      activeNodes.erase(id);

   mutexLock.unlock(); // U N L O C K

   return resultNode;
}

/**
 * Releases the old node and then references the next node.
 *
 * Note: Make sure to call this until it returns NULL (so the last node is released properly) or
 * remember to release the node yourself if you don't iterate to the end.
 *
 * @return NULL if oldNode was the last node
 */
Node* NodeStoreServers::referenceNextNodeAndReleaseOld(Node* oldNode)
{
   uint16_t oldNodeID = oldNode->getNumID();

   releaseNode(&oldNode);

   return referenceNextNode(oldNodeID);
}

/**
 * This is a helper to have only one call for the typical targetMapper->getNodeID() and following
 * referenceNode() calls.
 *
 * Note: remember to call releaseNode().
 *
 * @param targetMapper where to resolve the given targetID
 * @param outErr will be set to FhgfsOpsErr_UNKNOWNNODE, _UNKNOWNTARGET, _SUCCESS (may be NULL)
 * @return NULL if targetID is not mapped or if the mapped node does not exist in the store.
 */
Node* NodeStoreServers::referenceNodeByTargetID(uint16_t targetID, TargetMapper* targetMapper,
   FhgfsOpsErr* outErr)
{
   uint16_t nodeID = targetMapper->getNodeID(targetID);
   if(unlikely(!nodeID) )
   { // no such target mapping exists
      SAFE_ASSIGN(outErr, FhgfsOpsErr_UNKNOWNTARGET);
      return NULL;
   }

   Node* node = referenceNode(nodeID);
   if(unlikely(!node) )
   { // no node with this id exists
      SAFE_ASSIGN(outErr, FhgfsOpsErr_UNKNOWNNODE);
      return NULL;
   }

   SAFE_ASSIGN(outErr, FhgfsOpsErr_SUCCESS);
   return node;
}

/**
 * @param node double-pointer, because this method sets *node=NULL to prevent further usage of
 * of the pointer after release
 */
void NodeStoreServers::releaseNode(Node** node)
{
   SafeMutexLock mutexLock(&mutex); // L O C K

   releaseNodeUnlocked(node);

   mutexLock.unlock(); // U N L O C K
}

/**
 * Note: unlocked, so caller must hold lock
 *
 * @param node double-pointer, because this method sets *node=NULL to prevent further usage of
 * of the pointer after release
 */
void NodeStoreServers::releaseNodeUnlocked(Node** node)
{
   /* note: if node was neither active nor deleted, it might still be the localNode
      (so this case is not an error) */

   /* note: we must check deletedNodes here first, in case the deleted numID has been re-used
      by the mgmtd already (in which case we could release the wrong node otherwise) */

   NodePtrMapIter deletedIter = deletedNodes.find(*node);
   if(deletedIter != deletedNodes.end() )
   { // node marked for deletion => check refCount
      NodeReferencer* nodeRefer = deletedIter->second;

      int refCount = nodeRefer->release();
      if(!refCount)
      { // execute delayed deletion
         deletedNodes.erase(deletedIter);
         delete nodeRefer;
      }

      return;
   }

   NodeMapIter activeIter = activeNodes.find( (*node)->getNumID() );
   if(activeIter != activeNodes.end() )
   { // node active => just decrease refCount
      NodeReferencer* nodeRefer = activeIter->second;
      nodeRefer->release();
   }


   /* make sure noone can access the given node pointer anymore after release (just to avoid
      human errors) */
   *node = NULL;
}

/**
 * @return true if node was active before deletion
 */
bool NodeStoreServers::deleteNode(uint16_t id)
{
   SafeMutexLock mutexLock(&mutex); // L O C K

   bool nodeWasActive = false; // true if node was active before deletion

   NodeMapIter iter = activeNodes.find(id);
   if(iter != activeNodes.end() )
   { // node exists in active store

      NodeReferencer* nodeRefer = iter->second;
      Node* node = nodeRefer->getReferencedObject();

      if(nodeRefer->getRefCount() )
      { // node is referenced => move to deletedNodes
         deletedNodes.insert(NodePtrMapVal(node, iter->second) );
         activeNodes.erase(iter);
      }
      else
      { // no references => delete immediately
         activeNodes.erase(id);
         delete nodeRefer;
      }

      nodeWasActive = true;

      // forward removal to (optionally) attached objects

      if(capacityPools)
         capacityPools->remove(id); // remove node from capacity pools

      if(targetMapper)
         targetMapper->unmapByNodeID(id); // remove node from target mapper
   }

   mutexLock.unlock(); // U N L O C K

   return nodeWasActive;
}


/**
 * Note: remember to either call releaseAllNodes() or call releaseNode() for each node
 * in the list.
 */
void NodeStoreServers::referenceAllNodes(NodeList* outList)
{
   SafeMutexLock mutexLock(&mutex); // L O C K

   NodeReferencer localNodeRefer(localNode, false); /* don't move this into if-brackets, because it
      would be removed from stack then and we need to access it again further below.*/

   if(localNode) // insert local node (to provide consistent node list order)
      activeNodes.insert(NodeMapVal(localNode->getNumID(), &localNodeRefer) );

   for(NodeMapIter iter = activeNodes.begin(); iter != activeNodes.end(); iter++)
   {
      NodeReferencer* nodeRefer = iter->second;
      Node* node = nodeRefer->reference();

      outList->push_back(node);
   }

   if(localNode) // remove local node
      activeNodes.erase(localNode->getNumID() );

   mutexLock.unlock(); // U N L O C K
}

/**
 * @param list will be cleared after all nodes have been released
 */
void NodeStoreServers::releaseAllNodes(NodeList* list)
{
   SafeMutexLock mutexLock(&mutex); // L O C K

   for(NodeListIter iter = list->begin(); iter != list->end(); iter++)
   {
      Node* node = *iter;
      releaseNodeUnlocked(&node);
   }

   mutexLock.unlock(); // U N L O C K


   list->clear();
}

/**
 * Check whether a node exists in the (active) store.
 *
 * @return true if the node exists
 */
bool NodeStoreServers::isNodeActive(uint16_t id)
{
   SafeMutexLock mutexLock(&mutex); // L O C K

   bool isActive = false;

   if(localNode && (id == localNode->getNumID() ) )
      isActive = true;
   else
   { // search in active nodes map

      NodeMapIter iter = activeNodes.find(id);
      if(iter != activeNodes.end() )
         isActive = true; // node is in the active tree
   }

   mutexLock.unlock(); // U N L O C K

   return isActive;
}

/**
 * Get number of (active) nodes in store.
 *
 * note: returned size includes the localnode
 */
size_t NodeStoreServers::getSize()
{
   SafeMutexLock mutexLock(&mutex); // L O C K

   size_t nodesSize = localNode ? (activeNodes.size() + 1) : activeNodes.size();

   mutexLock.unlock();

   return nodesSize; // U N L O C K
}

uint16_t NodeStoreServers::getRootNodeNumID()
{
   SafeMutexLock mutexLock(&mutex); // L O C K

   uint16_t nodeNumID = rootNodeID;

   mutexLock.unlock(); // U N L O C K

   return nodeNumID;
}

/**
 * Set internal root node ID.
 *
 * @return false if the new ID was rejected (e.g. because we already had an id set and
 * ignoreExistingRoot was false).
 */
bool NodeStoreServers::setRootNodeNumID(uint16_t id, bool ignoreExistingRoot)
{
   // don't allow invalid id 0 (if not forced to do so)
   if(!id && !ignoreExistingRoot)
      return false;


   SafeMutexLock mutexLock(&mutex); // L O C K

   bool setRootRes = true;

   if(!this->rootNodeID)
   { // no root defined yet => set the new root
      this->rootNodeID = id;
   }
   else
   if(!ignoreExistingRoot)
   { // root defined already, reject new root
      setRootRes = false;
   }
   else
   { // root defined already, but shall be ignored
      this->rootNodeID = id;
   }

   mutexLock.unlock(); // U N L O C K

   return setRootRes;
}

/**
 * Waits for the first node that is added to the store.
 *
 * @return true if a new node was added to the store before the timeout expired
 */
bool NodeStoreServers::waitForFirstNode(int waitTimeoutMS)
{
   bool retVal;
   int elapsedMS;

   SafeMutexLock mutexLock(&mutex); // L O C K

   Time startT;
   elapsedMS = 0;

   while(!activeNodes.size() && (elapsedMS < waitTimeoutMS) )
   {
      newNodeCond.timedwait(&mutex, waitTimeoutMS - elapsedMS);

      elapsedMS = startT.elapsedMS();
   }

   retVal = (activeNodes.size() > 0);

   mutexLock.unlock(); // U N L O C K

   return retVal;
}


/**
 * Select random nodes from store.
 *
 * @param preferredNodes may be NULL
 */
void NodeStoreServers::chooseStorageNodes(unsigned numNodes, UInt16List* preferredNodes,
   UInt16Vector* outNodes)
{
   if(preferredNodes && !preferredNodes->empty() )
      chooseStorageNodesWithPref(numNodes, preferredNodes, false, outNodes);
   else
      chooseStorageNodesNoPref(numNodes, outNodes);
}


/**
 * @param outNodes might cotain less than numNodes if not enough nodes are known
 */
void NodeStoreServers::chooseStorageNodesNoPref(unsigned numNodes, UInt16Vector* outNodes)
{
   SafeMutexLock mutexLock(&mutex); // L O C K

   if(!localNode && !activeNodes.size() )
   { // there's nothing we can do without any storage targets
      mutexLock.unlock(); // U N L O C K
      return;
   }

   NodeReferencer localNodeRefer(localNode, false); /* don't move this into if-brackets, because it
      would be removed from stack then and we need to access it again further below.*/

   // temporary insertion of localNode to include it in the possible storage targets
   if(localNode)
      activeNodes.insert(NodeMapVal(localNode->getNumID(), &localNodeRefer) );

   unsigned nodesSize = activeNodes.size();

   if(numNodes > nodesSize)
      numNodes = nodesSize;

   NodeMapIter iter;
   int partSize = nodesSize / numNodes;

   moveIterToRandomElem<NodeMap, NodeMapIter>(activeNodes, iter);

   // for each node in numNodes
   for(unsigned i=0; i < numNodes; i++)
   {
      int rangeMin = i*partSize;
      int rangeMax = (i==(numNodes-1) ) ? (nodesSize-1) : (rangeMin + partSize - 1);
      // rangeMax decision because numNodes might not devide nodesSize without a remainder

      int next = randGen.getNextInRange(rangeMin, rangeMax);

      // move iter to the chosen node, add it to outNodes, and skip the remaining nodes of this part

      for(int j=rangeMin; j < next; j++)
         moveIterToNextRingElem<NodeMap, NodeMapIter>(activeNodes, iter);

      outNodes->push_back(iter->first);

      for(int j=next; j < (rangeMax+1); j++)
         moveIterToNextRingElem<NodeMap, NodeMapIter>(activeNodes, iter);
   }

   if(localNode) // remove local node
      activeNodes.erase(localNode->getNumID() );

   mutexLock.unlock(); // U N L O C K
}

/**
 * Note: This method assumes that there really are some preferred nodes and that those nodes
 * are probably active. So do not use it as a replacement for the version without preferred nodes!
 *
 * @param allowNonPreferredNodes true to enable use of non-preferred nodes if not enough
 *    preferred nodes are active to satisfy numNodes
 * @param outNodes might cotain less than numNodes if not enough nodes are known
 */
void NodeStoreServers::chooseStorageNodesWithPref(unsigned numNodes, UInt16List* preferredNodes,
   bool allowNonPreferredNodes, UInt16Vector* outNodes)
{
   SafeMutexLock mutexLock(&mutex); // L O C K

   if(!localNode && !activeNodes.size() )
   { // there's nothing we can do without any storage targets
      mutexLock.unlock(); // U N L O C K
      return;
   }

   NodeReferencer localNodeRefer(localNode, false); /* don't move this into if-brackets, because it
      would be removed from stack then and we need to access it again further below.*/

   // temporary insertion of localNode to include it in the possible storage targets
   if(localNode)
      activeNodes.insert(NodeMapVal(localNode->getNumID(), &localNodeRefer) );

   unsigned nodesSize = activeNodes.size();

   // max number of nodes is limited by the number of known active nodes
   if(numNodes > nodesSize)
      numNodes = nodesSize;

   // Stage 1: add all the preferred nodes that are actually available to the outNodes

   /* note: we use a separate map for the outNodes here to quickly find out (in stage 2) whether
      we already added a certain node from the preferred nodes (in stage 1) */

   IntSet outNodesSet;
   UInt16ListIter preferredIter;
   NodeMapIter activeNodesIter; // (will be re-used in stage 2)

   moveIterToRandomElem<UInt16List, UInt16ListIter>(*preferredNodes, preferredIter);

   // walk over all the preferred nodes and add them to outNodes when they are available
   // (note: iterTmp is just used to avoid calling preferredNodes->size() )
   for(UInt16ListIter iterTmp = preferredNodes->begin();
       (iterTmp != preferredNodes->end() ) && numNodes;
       iterTmp++)
   {
      activeNodesIter = activeNodes.find(*preferredIter);

      if(activeNodesIter != activeNodes.end() )
      { // this preferred node is active => add to outNodes and to outNodesSet
         outNodes->push_back(*preferredIter);
         outNodesSet.insert(*preferredIter);

         numNodes--;
      }

      moveIterToNextRingElem<UInt16List, UInt16ListIter>(*preferredNodes, preferredIter);
   }

   // Stage 2: add the remaining requested number of nodes from the active nodes

   /* if numNodes is greater than 0 then we have some requested nodes left, that could not be taken
      from the preferred nodes */

   /* we keep it simple here, because usually there will be enough preferred nodes available,
      so that this case is quite unlikely */

   if(allowNonPreferredNodes && numNodes)
   {
      IntSetIter outNodesSetIter;

      moveIterToRandomElem<NodeMap, NodeMapIter>(activeNodes, activeNodesIter);

      // while we haven't found the number of requested nodes
      while(numNodes)
      {
         outNodesSetIter = outNodesSet.find(activeNodesIter->first);
         if(outNodesSetIter == outNodesSet.end() )
         {
            outNodes->push_back(activeNodesIter->first);
            outNodesSet.insert(activeNodesIter->first);

            numNodes--;
         }

         moveIterToNextRingElem<NodeMap, NodeMapIter>(activeNodes, activeNodesIter);
      }
   }

   if(localNode) // remove local node
      activeNodes.erase(localNode->getNumID() );

   mutexLock.unlock(); // U N L O C K
}


/**
 * @param masterList must be ordered; contained nodes will be removed and may no longer be
 * accessed after calling this method.
 * @param updateExisting true to call addOrUpdate for nodes that already existed in the store
 * @param appLocalNode just what you get from app->getLocalNode(), to determine NIC capabilities
 */
void NodeStoreServers::syncNodes(NodeList* masterList, UInt16List* outAddedNumIDs,
   UInt16List* outRemovedNumIDs, bool updateExisting, Node* appLocalNode)
{
   // Note: We have two phases here:
   //    Phase 1 (locked): Identify added/removed nodes.
   //    Phase 2 (unlocked): Add/remove nodes from store.
   // This separation is required to not break compatibility with virtual overwritten add/remove
   // methods in derived classes (e.g. fhgfs_mgmtd).


   // P H A S E 1 (Identify added/removed nodes.)

   SafeMutexLock mutexLock(&mutex); // L O C K

   uint16_t localNodeNumID = 0; // 0 is invalid/unset, as usual for our numeric IDs
   NodeList addLaterNodes; // nodes to be added in phase 2
   NodeReferencer localNodeRefer(localNode, false); /* don't move this into if-brackets, because it
      would be removed from stack then and we need to access it again further below.*/

   // temporary insertion of localNode
   if(localNode)
   {
      localNodeNumID = localNode->getNumID();
      activeNodes.insert(NodeMapVal(localNodeNumID, &localNodeRefer) );
   }

   NodeMapIter activeIter = activeNodes.begin();
   NodeListIter masterIter = masterList->begin();

   while( (activeIter != activeNodes.end() ) && (masterIter != masterList->end() ) )
   {
      uint16_t currentActive = activeIter->first;
      uint16_t currentMaster = (*masterIter)->getNumID();

      if(currentMaster < currentActive)
      { // currentMaster is added
         outAddedNumIDs->push_back(currentMaster);
         addLaterNodes.push_back(*masterIter);

         masterList->pop_front();
         masterIter = masterList->begin();
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
         if(!updateExisting)
            delete(*masterIter);
         else
            addLaterNodes.push_back(*masterIter);

         masterList->pop_front();
         masterIter = masterList->begin();

         activeIter++;
      }
   }

   // remaining masterList nodes are added
   while(masterIter != masterList->end() )
   {
      outAddedNumIDs->push_back( (*masterIter)->getNumID() );
      addLaterNodes.push_back(*masterIter);

      masterList->pop_front();
      masterIter = masterList->begin();
   }

   // remaining active nodes are removed
   for( ; activeIter != activeNodes.end(); activeIter++)
   {
      if(likely(localNodeNumID != activeIter->first) ) // (don't ever remove localNode)
         outRemovedNumIDs->push_back(activeIter->first);
   }


   if(localNode) // remove temporarily inserted localNode
      activeNodes.erase(localNodeNumID);

   mutexLock.unlock(); // U N L O C K


   // P H A S E 2 (Add/remove nodes from store.)

   // remove nodes
   UInt16ListIter iter = outRemovedNumIDs->begin();
   while(iter != outRemovedNumIDs->end() )
   {
      uint16_t nodeID = *iter;
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
   for(NodeListIter iter = addLaterNodes.begin(); iter != addLaterNodes.end(); iter++)
   {
      Node* node = *iter;

      if(appLocalNode)
         node->getConnPool()->setLocalNicCaps(&localNicCaps);

      addOrUpdateNode(&node, NULL);
   }
}

/**
 * Nodes will automatically be added/removed from attached capacity pools when they are
 * added/removed from this store.
 */
void NodeStoreServers::attachCapacityPools(TargetCapacityPools* capacityPools)
{
   SafeMutexLock mutexLock(&mutex);

   this->capacityPools = capacityPools;

   mutexLock.unlock();
}

/**
 * Nodes will automatically be removed from attached target mapper when they are
 * removed from this store.
 */
void NodeStoreServers::attachTargetMapper(TargetMapper* targetMapper)
{
   SafeMutexLock mutexLock(&mutex);

   this->targetMapper = targetMapper;

   mutexLock.unlock();
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
uint16_t NodeStoreServers::generateNodeNumID(Node* node)
{
   // check whether this node's stringID is already associated with an active or deleted numID

   uint16_t previousNumID = retrieveNumIDFromStringID(node->getID() );
   if(previousNumID)
      return previousNumID;

   // check if we have a numeric ID left

   size_t numNodesTotal = activeNodes.size() + deletedNodes.size();

   if(unlikely(numNodesTotal >= NODESTORE_MAX_NODENUMIDS) )
      return 0; // all numeric IDs currently in use


   // try the quick path with a random numeric ID

   uint16_t newNumID = randGen.getNextInRange(1, NODESTORE_MAX_NODENUMIDS);

   bool isCollision = checkNodeNumIDCollision(newNumID);
   if(!isCollision)
      return newNumID; // we found an ID that no other node uses


   /* slow path: randomly picked ID is in use already, so walk from there to find the next free ID.
      (this could take quite some time if almost all numeric IDs are in use, but we assume that case
      is rare and we don't care too much about performance here.) */

   for( ; ; )
   {
      newNumID++;

      // check range... (avoid reserved value "0" as newNumID)
      if(unlikely(!newNumID || (newNumID > NODESTORE_MAX_NODENUMIDS) ) )
         newNumID = 1;

      bool isCollision = checkNodeNumIDCollision(newNumID);
      if(!isCollision)
         return newNumID; // we found an ID that no other node uses
   }
}

/**
 * Search activeNodes for a node with the given string ID and return it's associated numeric ID.
 *
 * Note: Caller must hold lock.
 *
 * @return 0 if no node with the given stringID was found, associated numeric ID otherwise.
 */
uint16_t NodeStoreServers::retrieveNumIDFromStringID(std::string nodeID)
{
   if(localNode && (nodeID == localNode->getID() ) )
      return localNode->getNumID();

   for(NodeMapIter iter = activeNodes.begin(); iter != activeNodes.end(); iter++)
   {
      Node* currentNode = iter->second->getReferencedObject();

      if(currentNode->getID() == nodeID)
         return currentNode->getNumID();
   }

   return 0;
}

/**
 * Check whether the given numeric ID is already being used.
 *
 * Note: Caller must hold lock.
 *
 * @return true if any of the active or deleted nodes already uses the given numeric ID, false
 * otherwise.
 */
bool NodeStoreServers::checkNodeNumIDCollision(uint16_t numID)
{
   if(localNode && (numID == localNode->getNumID() ) )
      return true;

   NodeMapIter activeIter = activeNodes.find(numID);
   if(activeIter != activeNodes.end() )
      return true; // we found a node with the given numID

   for(NodePtrMapIter deletedIter = deletedNodes.begin();
       deletedIter != deletedNodes.end();
       deletedIter++)
   {
      Node* currentNode = deletedIter->second->getReferencedObject();

      if(numID == currentNode->getNumID() )
         return true; // we found a node with the given numID
   }

   return false; // no node uses the given numID
}

/**
 * Get complete node IDs and type for log messages from numerical ID.
 *
 * Note: This method is expensive, so use this function only in performance uncritical code paths.
 *       If you already have a Node object, call node->getNodeIDWithTypeStr() directly.
 */
std::string NodeStoreServers::getNodeIDWithTypeStr(uint16_t numID)
{
   SafeMutexLock mutexLock(&mutex); // L O C K

   std::string nodeID;

   NodeMapIter iter = activeNodes.find(numID);
   if(iter != activeNodes.end() )
   { // node exists in active store

      NodeReferencer* nodeRefer = iter->second;
      Node* node = nodeRefer->getReferencedObject();

      nodeID = node->getNodeIDWithTypeStr();
   }
   else
   if(localNode && (numID == localNode->getNumID() ) )
      nodeID = localNode->getNodeIDWithTypeStr();
   else
      nodeID = std::string("Unknown node: ") +
         Node::nodeTypeToStr(storeType) + " [ID: " + StringTk::uintToStr(numID) + "]";

   mutexLock.unlock(); // U N L O C K

   return nodeID;
}

