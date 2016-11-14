#include <common/threading/SafeMutexLock.h>
#include <common/app/log/LogContext.h>
#include "NodeStoreClients.h"

/**
 * Note: Does not initialize the localNode data (the localNode can be set later)
 *
 * @param channelsDirectDefault false to make all channels indirect by default (only metadata
 *    server should set this to true, all others to false)
 */
NodeStoreClients::NodeStoreClients(bool channelsDirectDefault) throw(InvalidConfigException) :
   AbstractNodeStore(NODETYPE_Client)
{
   this->localNode = NULL;
   this->channelsDirectDefault = channelsDirectDefault;
}


NodeStoreClients::~NodeStoreClients()
{
   for(NodeMapIter iter = activeNodes.begin(); iter != activeNodes.end(); iter++)
      delete(iter->second);

   activeNodes.clear();

   for(NodeMapIter iter = deletedNodes.begin(); iter != deletedNodes.end(); iter++)
      delete(iter->second);

   deletedNodes.clear();
}


/**
 * @param node belongs to the store after calling this method; this method will set (*node=NULL);
 * so do not free it and don't use it any more afterwards (reference it from this store if you need
 * it)
 * @return true if the node was not in the active group yet, false otherwise
 */
bool NodeStoreClients::addOrUpdateNode(Node** node)
{
   std::string nodeID = (*node)->getID();

   if(localNode && (nodeID == localNode->getID() ) )
   { // we don't allow to update local node this way (would lead to problems during sync etc.)
      delete(*node);
      *node = NULL;
      return false;
   }


   SafeMutexLock mutexLock(&mutex); // L O C K

   // is node in any of the stores already?

   NodeMapIter iter = activeNodes.find(nodeID);
   bool nodeWasActive = (iter != activeNodes.end() );

   if(nodeWasActive)
   { // node was in the store already => update it
      NodeReferencer* nodeRefer = iter->second;
      NicAddressList nicList( (*node)->getNicList() );

      nodeRefer->getReferencedObject()->updateLastHeartbeatT();
      nodeRefer->getReferencedObject()->setFhgfsVersion( (*node)->getFhgfsVersion() );
      nodeRefer->getReferencedObject()->updateInterfaces(
         (*node)->getPortUDP(), (*node)->getPortTCP(), nicList);

      delete(*node);
   }
   else
   if( (iter = deletedNodes.find(nodeID) ) != deletedNodes.end() )
   { // node was deleted => update and re-activate it
      NodeReferencer* nodeRefer = iter->second;
      Node* nodeNonRef = nodeRefer->getReferencedObject();
      NicAddressList nicList( (*node)->getNicList() );

      nodeNonRef->updateLastHeartbeatT();
      nodeNonRef->updateInterfaces( (*node)->getPortUDP(), (*node)->getPortTCP(), nicList);

      delete(*node);

      activeNodes.insert(NodeMapVal(nodeID, iter->second) );
      deletedNodes.erase(iter);

      newNodeCond.broadcast();
   }
   else
   { // node not in any store yet
      activeNodes.insert(NodeMapVal(nodeID, new NodeReferencer(*node) ) );

      (*node)->getConnPool()->setChannelDirect(channelsDirectDefault);

      newNodeCond.broadcast();
   }

   mutexLock.unlock(); // U N L O C K

   *node = NULL;

   return !nodeWasActive;
}

bool NodeStoreClients::updateLastHeartbeatT(std::string nodeID)
{
   Node* node = referenceNode(nodeID);
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
Node* NodeStoreClients::referenceNode(std::string id)
{
   Node* node;

   SafeMutexLock mutexLock(&mutex);

   NodeMapIter iter = activeNodes.find(id);
   if(iter == activeNodes.end() )
   { // not found, check if it's the local node
      if(localNode && !id.compare(localNode->getID() ) )
         node = localNode;
      else
         node =  NULL;
   }
   else
   {
      NodeReferencer* nodeRefer = iter->second;
      node = nodeRefer->reference();


      #ifdef FHGFS_DEBUG

      if(nodeRefer->getRefCount() > 100)
      {
         LogContext("NodeStoreClients::referenceNode").log(1,
            std::string("WARNING: Lots of references to node (=> leak?): ") +
            node->getID() + " ref count: " + StringTk::intToStr(nodeRefer->getRefCount() ) );
      }

      #endif // FHGFS_DEBUG
   }

   mutexLock.unlock();

   return node;
}

/**
 * Note: remember to call releaseNode()
 *
 * @return can be NULL if localNode is not included
 */
Node* NodeStoreClients::referenceFirstNode()
{
   Node* resultNode = NULL;

   SafeMutexLock mutexLock(&mutex);

   NodeReferencer localNodeRefer(localNode, false); /* don't move this into if-brackets, because it
      would be removed from stack then and we need to access it again further below.*/

   if(localNode)
      activeNodes.insert(NodeMapVal(localNode->getID(), &localNodeRefer) );

   NodeMapIter iter = activeNodes.begin();

   if(iter != activeNodes.end() )
   {
      NodeReferencer* nodeRefer = iter->second;
      resultNode = nodeRefer->reference();
   }

   if(localNode)
      activeNodes.erase(localNode->getID() );

   mutexLock.unlock();

   return resultNode;
}

/**
 * Note: remember to call releaseNode()
 *
 * @return NULL if nodeID was the last node
 */
Node* NodeStoreClients::referenceNextNode(std::string nodeID)
{
   Node* resultNode;
   bool nodeIDexisted = true;

   SafeMutexLock mutexLock(&mutex);

   // find next by inserting localNode and nodeID param (if it didn't exist already) and moving
   // the iterator to the next node after nodeID

   bool insertLocalNode = (localNode != NULL) && (nodeID <= localNode->getID() ); // note: if nodeID
      // is already larger than localNodeID, then localNode can't possibly be the next node

   NodeReferencer localNodeRefer(localNode, false); /* don't move this into if-brackets, because it
      would be removed from stack then and we need to access it again further below.*/

   if(insertLocalNode)
      activeNodes.insert(NodeMapVal(localNode->getID(), &localNodeRefer) );

   NodeMapIter iter = activeNodes.find(nodeID);
   if(iter == activeNodes.end() )
   { // nodeID doesn't exist => insert it
      iter = activeNodes.insert(NodeMapVal(nodeID, NULL) ).first;
      nodeIDexisted = false;
   }

   iter++; // move to next

   if(iter == activeNodes.end() )
   {
      resultNode = NULL;
   }
   else
   {
      NodeReferencer* nodeRefer = iter->second;
      resultNode = nodeRefer->reference();
   }

   if(insertLocalNode)
      activeNodes.erase(localNode->getID() );

   if(!nodeIDexisted)
      activeNodes.erase(nodeID);

   mutexLock.unlock();

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
Node* NodeStoreClients::referenceNextNodeAndReleaseOld(Node* oldNode)
{
   std::string oldNodeID = oldNode->getID();

   releaseNode(&oldNode);

   return referenceNextNode(oldNodeID);
}

/**
 * @param node double-pointer, because this method sets *node=NULL to prevent further usage of
 * of the pointer after release
 */
void NodeStoreClients::releaseNode(Node** node)
{
   SafeMutexLock mutexLock(&mutex);

   releaseNodeUnlocked(node);

   mutexLock.unlock();
}

/**
 * note: unlocked, so caller must hold lock
 *
 * @param node double-pointer, because this method sets *node=NULL to prevent further usage of
 * of the pointer after release
 */
void NodeStoreClients::releaseNodeUnlocked(Node** node)
{
   std::string nodeID = (*node)->getID();

   /* note: if node is neither active nor deleted, it might still be the localNode
      (so this case is not an error) */

   NodeMapIter iter = activeNodes.find(nodeID);
   if(iter != activeNodes.end() )
   { // node active => just decrease refCount
      NodeReferencer* nodeRefer = iter->second;
      nodeRefer->release();
   }
   else
   if( (iter = deletedNodes.find(nodeID) ) != deletedNodes.end() )
   { // node marked for deletion => check refCount
      NodeReferencer* nodeRefer = iter->second;
      int refCount = nodeRefer->release();

      if(!refCount)
      { // execute delayed deletion
         deletedNodes.erase(iter);
         delete nodeRefer;
      }
   }

   /* make sure noone can access the given node pointer anymore after release (just to avoid
      human errors) */
   *node = NULL;
}

/**
 * @return true if node was active before deletion
 */
bool NodeStoreClients::deleteNode(std::string nodeID)
{
   bool nodeWasActive = false; // true if node was active before deletion

   SafeMutexLock mutexLock(&mutex);

   NodeMapIter iter = activeNodes.find(nodeID);
   if(iter != activeNodes.end() )
   {
      NodeReferencer* nodeRefer = iter->second;

      if(nodeRefer->getRefCount() )
      { // node is referenced => move to deletedNodes
         deletedNodes.insert(NodeMapVal(nodeID, iter->second) );
         activeNodes.erase(iter);
      }
      else
      { // no references => delete immediately
         activeNodes.erase(nodeID);
         delete nodeRefer;
      }

      nodeWasActive = true;
   }

   mutexLock.unlock();

   return nodeWasActive;
}


/**
 * Note: remember to either call releaseAllNodes() or call releaseNode() for each node
 * in the list.
 */
void NodeStoreClients::referenceAllNodes(NodeList* outList)
{
   SafeMutexLock mutexLock(&mutex);

   NodeReferencer localNodeRefer(localNode, false); /* don't move this into if-brackets, because it
      would be removed from stack then and we need to access it again further below.*/

   if(localNode)
      activeNodes.insert(NodeMapVal(localNode->getID(), &localNodeRefer) );

   NodeMapIter iter;

   for(iter = activeNodes.begin(); iter != activeNodes.end(); iter++)
   {
      NodeReferencer* nodeRefer = iter->second;
      Node* node = nodeRefer->reference();

      outList->push_back(node);
   }

   if(localNode)
      activeNodes.erase(localNode->getID() );

   mutexLock.unlock();
}

/**
 * @param list will be cleared after all nodes have been released
 */
void NodeStoreClients::releaseAllNodes(NodeList* list)
{
   SafeMutexLock mutexLock(&mutex);

   NodeListIter iter;

   for(iter = list->begin(); iter != list->end(); iter++)
   {
      Node* node = *iter;
      releaseNodeUnlocked(&node);
   }

   mutexLock.unlock();


   list->clear();
}

bool NodeStoreClients::isNodeActive(std::string nodeID)
{
   bool isActive = false;

   SafeMutexLock mutexLock(&mutex);

   NodeMapIter iter = activeNodes.find(nodeID);
   if(iter == activeNodes.end() )
   { // not found, check if it's the local node
      if(localNode && !nodeID.compare(localNode->getID() ) )
         isActive = true;
   }
   else
   { // node is in the active tree
      isActive = true;
   }

   mutexLock.unlock();

   return isActive;
}

/**
 * Get number of (active) nodes in store.
 *
 * note: returned size includes the localnode
 */
size_t NodeStoreClients::getSize()
{
   SafeMutexLock mutexLock(&mutex);

   size_t nodesSize = localNode ? (activeNodes.size() + 1) : activeNodes.size();

   mutexLock.unlock();

   return nodesSize;
}

/**
 * @param masterList must be ordered; contained nodes will be removed and may no longer be
 * accessed after calling this method.
 * @param updateExisting true to call addOrUpdate for nodes that already existed in the store
 * @param appLocalNode (just what you get from app->getLocalNode() )
 */
void NodeStoreClients::syncNodes(NodeList* masterList, StringList* outAddedIDs, StringList* outRemovedIDs,
   bool updateExisting, Node* appLocalNode)
{
   // Note: We have two phases here:
   //    Phase 1 (locked): Identify added/removed nodes.
   //    Phase 2 (unlocked): Add/remove nodes from store.
   // This separation is required to not break compatibility with virtual overwritten add/remove
   // methods in derived classes (e.g. fhgfs_mgmtd).


   // P H A S E 1 (Identify added/removed nodes.)

   std::string localNodeID;
   NodeList addLaterNodes; // nodes to be added in phase 2

   SafeMutexLock mutexLock(&mutex);

   NodeReferencer localNodeRefer(localNode, false); /* don't move this into if-brackets, because it
      would be removed from stack then and we need to access it again further below.*/

   // temporary insertion of localNode
   if(localNode)
   {
      localNodeID = localNode->getID();
      activeNodes.insert(NodeMapVal(localNode->getID(), &localNodeRefer) );
   }

   NodeMapIter activeIter = activeNodes.begin();
   NodeListIter masterIter = masterList->begin();

   while( (activeIter != activeNodes.end() ) && (masterIter != masterList->end() ) )
   {
      std::string currentActive(activeIter->first);
      std::string currentMaster( (*masterIter)->getID() );

      if(currentMaster < currentActive)
      { // currentMaster is added
         outAddedIDs->push_back(currentMaster);
         addLaterNodes.push_back(*masterIter);

         masterList->pop_front();
         masterIter = masterList->begin();
      }
      else
      if(currentActive < currentMaster)
      { // currentActive is removed (but don't ever remove localNode)
         if(likely(localNodeID != currentActive) )
            outRemovedIDs->push_back(currentActive);

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
      outAddedIDs->push_back( (*masterIter)->getID() );
      addLaterNodes.push_back(*masterIter);

      masterList->pop_front();
      masterIter = masterList->begin();
   }

   // remaining active nodes are removed
   for( ; activeIter != activeNodes.end(); activeIter++)
   {
      if(likely(localNodeID != activeIter->first) ) // (don't ever remove localNode)
         outRemovedIDs->push_back(activeIter->first);
   }


   // remove temporarily inserted localNode
   if(localNode)
      activeNodes.erase(localNode->getID() );

   mutexLock.unlock();


   // P H A S E 2 (Add/remove nodes from store.)

   // remove nodes
   StringListIter iter = outRemovedIDs->begin();
   while(iter != outRemovedIDs->end() )
   {
      std::string nodeID(*iter);
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

      addOrUpdateNode(&node);
   }
}

