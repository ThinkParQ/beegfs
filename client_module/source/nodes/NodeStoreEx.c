#include <common/net/sock/NetworkInterfaceCard.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/nodes/NodeListIter.h>
#include <common/system/System.h>
#include <os/OsCompat.h>
#include "NodeStoreEx.h"

#define NODESTORE_WARN_REFNUM 2000

/**
 * @param storeType will be applied to nodes on addOrUpdate()
 */
void NodeStoreEx_init(NodeStoreEx* this, App* app, NodeType storeType)
{
   this->app = app;

   RWLock_init(&this->rwLock);

   NodeTree_init(&this->nodeTree);

   this->newNodeAppeared = NULL;

   this->_rootOwner = NodeOrGroup_fromGroup(0); // 0 means undefined/invalid

   this->storeType = storeType;
}

NodeStoreEx* NodeStoreEx_construct(App* app, NodeType storeType)
{
   NodeStoreEx* this = (NodeStoreEx*)os_kmalloc(sizeof(*this) );

   NodeStoreEx_init(this, app, storeType);

   return this;
}

void NodeStoreEx_uninit(NodeStoreEx* this)
{
   NodeTree_uninit(&this->nodeTree);
}

void NodeStoreEx_destruct(NodeStoreEx* this)
{
   NodeStoreEx_uninit(this);

   kfree(this);
}


/**
 * @param node belongs to the store after calling this method; this method will set (*node=NULL);
 * so do not free it and don't use it any more afterwards (reference it from this store if you need
 * it)
 * @return true if the node was not in the store yet, false otherwise
 */
bool NodeStoreEx_addOrUpdateNode(NodeStoreEx* this, Node** node)
{
   const char* logContext = __func__;
   Logger* log = App_getLogger(this->app);

   NumNodeID nodeNumID = Node_getNumID(*node);
   char* nodeID = Node_getID(*node);

   Node* active;

   // check if numeric ID is defined

   if(unlikely(!nodeNumID.value) )
   { // undefined numeric ID should never happen
      Logger_logErrFormatted(log, logContext,
         "Rejecting node with undefined numeric ID: %s; Type: %s",
         nodeID, Node_nodeTypeToStr(this->storeType) );

      Node_put(*node);
      *node = NULL;
      return false;
   }

   RWLock_writeLock(&this->rwLock); // L O C K

   // is node in any of the stores already?

   active = NodeTree_find(&this->nodeTree, nodeNumID);

   if(active)
   { // node was in the store already => update it
      if(unlikely(strcmp(nodeID, Node_getID(active) ) ) )
      { // bad: numeric ID collision for two different node string IDs
         Logger_logErrFormatted(log, logContext,
            "Numeric ID collision for two different string IDs: %s / %s; Type: %s",
            nodeID, Node_getID(active), Node_nodeTypeToStr(this->storeType) );
      }
      else
      { // update heartbeat time of existing node
         Node_updateLastHeartbeatT(active);
         Node_updateInterfaces(active, Node_getPortUDP(*node), Node_getPortTCP(*node),
            Node_getNicList(*node) );
      }

      Node_put(*node);
   }
   else
   { // node is not currently active => insert it
      NodeTree_insert(&this->nodeTree, nodeNumID, *node);

      #ifdef BEEGFS_DEBUG
      // check whether this node type and store type differ
      if( (Node_getNodeType(*node) != NODETYPE_Invalid) &&
          (Node_getNodeType(*node) != this->storeType) )
      {
         Logger_logErrFormatted(log, logContext,
            "Node type and store type differ. Node: %s %s; Store: %s",
            Node_getNodeTypeStr(*node), nodeID, Node_nodeTypeToStr(this->storeType) );
      }
      #endif // BEEGFS_DEBUG

      Node_setIsActive(*node, true);
      Node_setNodeType(*node, this->storeType);

      __NodeStoreEx_handleNodeVersion(this, *node);

      if(this->newNodeAppeared)
         complete(this->newNodeAppeared);
   }

   RWLock_writeUnlock(&this->rwLock); // U N L O C K

   *node = NULL;

   return !active;
}

/**
 * Note: remember to call releaseNode()
 *
 * @return NULL if no such node exists
 */
Node* NodeStoreEx_referenceNode(NodeStoreEx* this, NumNodeID id)
{
   Logger* log = App_getLogger(this->app);
   Node* node = NULL;

   // check for invalid id 0
   #ifdef BEEGFS_DEBUG
   if(!id.value)
   {
      Logger_log(log, Log_CRITICAL, __func__, "BUG?: Attempt to reference numeric node ID '0'");
      dump_stack();
   }
   #endif // BEEGFS_DEBUG

   IGNORE_UNUSED_VARIABLE(log);

   RWLock_readLock(&this->rwLock); // L O C K

   node = NodeTree_find(&this->nodeTree, id);
   if (likely(node))
   { // found it
      unsigned refs;
      Node_get(node);

      (void) refs;
      // check for unusually high reference count
#ifdef BEEGFS_DEBUG
# ifdef KERNEL_HAS_KREF_READ
      refs = kref_read(&node->references);
# else
      refs = atomic_read(&node->references.refcount);
#endif

      if (refs > NODESTORE_WARN_REFNUM)
         Logger_logFormatted(log, Log_CRITICAL, __func__,
            "WARNING: Lots of references to node (=> leak?): %s %s; ref count: %d",
            Node_getNodeTypeStr(node), Node_getID(node), refs);
#endif // BEEGFS_DEBUG
   }

   RWLock_readUnlock(&this->rwLock); // U N L O C K

   return node;
}

/**
 * Note: remember to call releaseNode()
 *
 * @return NULL if no such node exists
 */
Node* NodeStoreEx_referenceRootNode(NodeStoreEx* this, NodeOrGroup* rootID)
{
   Node* node = NULL;
   MirrorBuddyGroupMapper* metaBuddyGroupMapper = App_getMetaBuddyGroupMapper(this->app);
   NumNodeID nodeID;

   RWLock_readLock(&this->rwLock); // L O C K

   *rootID = this->_rootOwner;
   if (this->_rootOwner.isGroup)
      nodeID.value = MirrorBuddyGroupMapper_getPrimaryTargetID(metaBuddyGroupMapper,
            this->_rootOwner.node.value);
   else
      nodeID = this->_rootOwner.node;

   node = NodeTree_find(&this->nodeTree, nodeID);
   if(likely(node) )
      Node_get(node);

   RWLock_readUnlock(&this->rwLock); // U N L O C K

   return node;
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
Node* NodeStoreEx_referenceNodeByTargetID(NodeStoreEx* this, uint16_t targetID,
   TargetMapper* targetMapper, FhgfsOpsErr* outErr)
{
   NumNodeID nodeID;
   Node* node;

   nodeID = TargetMapper_getNodeID(targetMapper, targetID);
   if(!nodeID.value)
   {
      SAFE_ASSIGN(outErr, FhgfsOpsErr_UNKNOWNTARGET);
      return NULL;
   }

   node = NodeStoreEx_referenceNode(this, nodeID);

   if(!node)
   {
      SAFE_ASSIGN(outErr, FhgfsOpsErr_UNKNOWNNODE);
      return NULL;
   }

   SAFE_ASSIGN(outErr, FhgfsOpsErr_SUCCESS);
   return node;
}

/**
 * @return true if node existed as active node
 */
bool NodeStoreEx_deleteNode(NodeStoreEx* this, NumNodeID nodeID)
{
   const char* logContext = __func__;
   Logger* log = App_getLogger(this->app);

   bool nodeWasActive;

   #ifdef BEEGFS_DEBUG
   if(unlikely(!nodeID.value) ) // should never happen
      Logger_logFormatted(log, Log_CRITICAL, logContext, "Called with invalid node ID '0'");
   #endif // BEEGFS_DEBUG

   IGNORE_UNUSED_VARIABLE(logContext);
   IGNORE_UNUSED_VARIABLE(log);

   RWLock_writeLock(&this->rwLock); // L O C K

   nodeWasActive = NodeTree_erase(&this->nodeTree, nodeID);

   RWLock_writeUnlock(&this->rwLock); // U N L O C K

   return nodeWasActive;
}

unsigned NodeStoreEx_getSize(NodeStoreEx* this)
{
   unsigned nodesSize;

   RWLock_readLock(&this->rwLock); // L O C K

   nodesSize = this->nodeTree.size;

   RWLock_readUnlock(&this->rwLock); // U N L O C K

   return nodesSize;
}

/**
 * This is used to iterate over all stored nodes.
 * Start with this and then use referenceNextNode() until it returns NULL.
 *
 * Note: remember to call releaseNode()
 *
 * @return can be NULL
 */
Node* NodeStoreEx_referenceFirstNode(NodeStoreEx* this)
{
   NodeTreeIter iter;
   Node* resultNode = NULL;

   RWLock_readLock(&this->rwLock); // L O C K

   NodeTreeIter_init(&iter, &this->nodeTree);
   if (!NodeTreeIter_end(&iter) )
   {
      resultNode = NodeTreeIter_value(&iter);
      Node_get(resultNode);
   }

   RWLock_readUnlock(&this->rwLock); // U N L O C K

   return resultNode;
}

/**
 * Note: remember to call releaseNode()
 *
 * @return NULL if nodeID was the last node
 */
Node* NodeStoreEx_referenceNextNodeAndReleaseOld(NodeStoreEx* this, Node* oldNode)
{
   Node* result = NULL;

   RWLock_readLock(&this->rwLock); // L O C K

   result = NodeTree_getNext(&this->nodeTree, oldNode);
   if (result)
      Node_get(result);

   Node_put(oldNode);

   RWLock_readUnlock(&this->rwLock); // U N L O C K

   return result;
}

/**
 * @return 0 if no root node is known
 */
NodeOrGroup NodeStoreEx_getRootOwner(NodeStoreEx* this)
{
   NodeOrGroup owner;

   RWLock_readLock(&this->rwLock); // L O C K

   owner = this->_rootOwner;

   RWLock_readUnlock(&this->rwLock); // U N L O C K

   return owner;
}

/**
 * Set internal root node ID.
 *
 * @return false if the new ID was rejected (e.g. because we already had an id set and
 * ignoreExistingRoot was false).
 */
bool NodeStoreEx_setRootOwner(NodeStoreEx* this, NodeOrGroup owner,
   bool ignoreExistingRoot)
{
   bool setRootRes = true;

   // don't allow invalid id 0 (if not forced to do so)
   if(!owner.group && !ignoreExistingRoot)
      return false;

   RWLock_writeLock(&this->rwLock); // L O C K

   if (!NodeOrGroup_valid(this->_rootOwner))
   { // rootID empty => set the new root
      this->_rootOwner = owner;
   }
   else if (!ignoreExistingRoot)
   { // root defined already, reject new root
      setRootRes = false;
   }
   else
   { // root defined already, but shall be ignored
      this->_rootOwner = owner;
   }

   RWLock_writeUnlock(&this->rwLock); // U N L O C K

   return setRootRes;
}

/**
 * Waits for the first node that is added to the store.
 *
 * @return true when a new node was added to the store before the timeout expired
 */
bool NodeStoreEx_waitForFirstNode(NodeStoreEx* this, int waitTimeoutMS)
{
   bool retVal = false;
   struct completion cond;

   RWLock_readLock(&this->rwLock); // L O C K

   retVal = this->nodeTree.size > 0;

   RWLock_readUnlock(&this->rwLock); // U N L O C K

   if(retVal)
      return retVal;

   RWLock_writeLock(&this->rwLock); // L O C K

   WARN_ON(this->newNodeAppeared);
   init_completion(&cond);
   this->newNodeAppeared = &cond;

   RWLock_writeUnlock(&this->rwLock); // U N L O C K

   /* may time out or not, we don't care. activeCount is what's important */
   wait_for_completion_timeout(&cond, TimeTk_msToJiffiesSchedulable(waitTimeoutMS) );

   RWLock_writeLock(&this->rwLock); // L O C K

   this->newNodeAppeared = NULL;
   retVal = this->nodeTree.size > 0;

   RWLock_writeUnlock(&this->rwLock); // U N L O C K

   return retVal;
}


/**
 * @param masterList must be ordered; contained nodes will be removed and may no longer be
 * accessed after calling this method.
 * @param appLocalNode just what you get from app->getLocalNode(), to determine NIC capabilities
 */
void NodeStoreEx_syncNodes(NodeStoreEx* this, NodeList* masterList, NumNodeIDList* outAddedIDs,
   NumNodeIDList* outRemovedIDs, Node* appLocalNode)
{
   // Note: We have two phases here:
   //    Phase 1 (locked): Identify added/removed nodes.
   //    Phase 2 (unlocked): Add/remove nodes from store.
   // This separation is required to not break compatibility with virtual overwritten add/remove
   // methods in derived classes (e.g. fhgfs_mgmtd).


   // P H A S E 1 (Identify added/removed nodes.)

   NodeTreeIter activeIter;
   NodeListIter masterIter;

   NumNodeIDListIter removedIDsIter;
   NodeList addLaterNodes; // nodes to be added in phase 2
   NodeListIter addLaterIter;

   NicListCapabilities localNicCaps;

   NodeList_init(&addLaterNodes);


   RWLock_writeLock(&this->rwLock); // L O C K

   NodeTreeIter_init(&activeIter, &this->nodeTree);
   NodeListIter_init(&masterIter, masterList);

   while(!NodeTreeIter_end(&activeIter) && !NodeListIter_end(&masterIter) )
   {
      Node* active = NodeTreeIter_value(&activeIter);
      NumNodeID currentActive = Node_getNumID(active);
      NumNodeID currentMaster = Node_getNumID(NodeListIter_value(&masterIter) );

      if(currentMaster.value < currentActive.value)
      { // currentMaster is added
         NumNodeIDList_append(outAddedIDs, currentMaster);
         NodeList_append(&addLaterNodes, NodeListIter_value(&masterIter) );

         NodeList_removeHead(masterList);
         NodeListIter_init(&masterIter, masterList);
      }
      else
      if(currentActive.value < currentMaster.value)
      { // currentActive is removed
         NumNodeIDList_append(outRemovedIDs, currentActive);
         NodeTreeIter_next(&activeIter);
      }
      else
      { // node unchanged
         NodeList_append(&addLaterNodes, NodeListIter_value(&masterIter) );

         NodeTreeIter_next(&activeIter);

         NodeList_removeHead(masterList);
         NodeListIter_init(&masterIter, masterList);
      }
   }

   // remaining masterList nodes are added
   while(!NodeListIter_end(&masterIter) )
   {
      NumNodeID currentMaster = Node_getNumID(NodeListIter_value(&masterIter) );

      NumNodeIDList_append(outAddedIDs, currentMaster);
      NodeList_append(&addLaterNodes, NodeListIter_value(&masterIter) );

      NodeList_removeHead(masterList);
      NodeListIter_init(&masterIter, masterList);
   }

   // remaining active nodes are removed
   for(; !NodeTreeIter_end(&activeIter); NodeTreeIter_next(&activeIter) )
   {
      Node* active = NodeTreeIter_value(&activeIter);

      NumNodeIDList_append(outRemovedIDs, Node_getNumID(active) );
   }

   RWLock_writeUnlock(&this->rwLock); // U N L O C K


   // P H A S E 2 (Add/remove nodes from store.)

   // remove nodes
   NumNodeIDListIter_init(&removedIDsIter, outRemovedIDs);

   while(!NumNodeIDListIter_end(&removedIDsIter) )
   {
      NumNodeID nodeID = NumNodeIDListIter_value(&removedIDsIter);
      NumNodeIDListIter_next(&removedIDsIter); // (removal invalidates iter)

      NodeStoreEx_deleteNode(this, nodeID);
   }

   // set local nic capabilities
   if(appLocalNode)
   {
      NicAddressList* localNicList = Node_getNicList(appLocalNode);
      NIC_supportedCapabilities(localNicList, &localNicCaps);
   }

   // add nodes
   NodeListIter_init(&addLaterIter, &addLaterNodes);

   for(; !NodeListIter_end(&addLaterIter); NodeListIter_next(&addLaterIter) )
   {
      Node* node = NodeListIter_value(&addLaterIter);

      if(appLocalNode)
         NodeConnPool_setLocalNicCaps(Node_getConnPool(node), &localNicCaps);

      NodeStoreEx_addOrUpdateNode(this, &node);
   }

   NodeList_uninit(&addLaterNodes);
}

/**
 * Take special actions based on version of a (typically new) node, e.g. compat flags deactivation.
 *
 * Note: Caller must hold lock.
 */
void __NodeStoreEx_handleNodeVersion(NodeStoreEx* this, Node* node)
{
   // nothing to be done here currently
}
