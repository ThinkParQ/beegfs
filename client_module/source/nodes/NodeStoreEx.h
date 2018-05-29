#ifndef NODESTOREEX_H_
#define NODESTOREEX_H_

#include <app/log/Logger.h>
#include <app/App.h>
#include <common/toolkit/list/NumNodeIDList.h>
#include <common/toolkit/list/UInt16List.h>
#include <common/storage/EntryInfo.h>
#include <common/storage/StorageErrors.h>
#include <common/threading/Condition.h>
#include <common/threading/RWLock.h>
#include <common/toolkit/StringTk.h>
#include <common/nodes/Node.h>
#include <common/nodes/NodeList.h>
#include <common/nodes/NodeTree.h>
#include <common/nodes/TargetMapper.h>
#include <common/Common.h>
#include <linux/completion.h>
#include <linux/rwsem.h>


struct NodeStoreEx;
typedef struct NodeStoreEx NodeStoreEx;


extern void NodeStoreEx_init(NodeStoreEx* this, App* app, NodeType storeType);
extern NodeStoreEx* NodeStoreEx_construct(App* app, NodeType storeType);
extern void NodeStoreEx_uninit(NodeStoreEx* this);
extern void NodeStoreEx_destruct(NodeStoreEx* this);

extern bool NodeStoreEx_addOrUpdateNode(NodeStoreEx* this, Node** node);
extern Node* NodeStoreEx_referenceNode(NodeStoreEx* this, NumNodeID id);
extern Node* NodeStoreEx_referenceRootNode(NodeStoreEx* this, NodeOrGroup* rootID);
extern Node* NodeStoreEx_referenceNodeByTargetID(NodeStoreEx* this, uint16_t targetID,
   TargetMapper* targetMapper, FhgfsOpsErr* outErr);
extern bool NodeStoreEx_deleteNode(NodeStoreEx* this, NumNodeID nodeID);
extern unsigned NodeStoreEx_getSize(NodeStoreEx* this);

extern Node* NodeStoreEx_referenceFirstNode(NodeStoreEx* this);
extern Node* NodeStoreEx_referenceNextNode(NodeStoreEx* this, NumNodeID nodeID);
extern Node* NodeStoreEx_referenceNextNodeAndReleaseOld(NodeStoreEx* this, Node* oldNode);

extern NodeOrGroup NodeStoreEx_getRootOwner(NodeStoreEx* this);
extern bool NodeStoreEx_setRootOwner(NodeStoreEx* this, NodeOrGroup owner,
   bool ignoreExistingRoot);

extern bool NodeStoreEx_waitForFirstNode(NodeStoreEx* this, int waitTimeoutMS);

extern void NodeStoreEx_syncNodes(NodeStoreEx* this, NodeList* masterList, NumNodeIDList* outAddedIDs,
   NumNodeIDList* outRemovedIDs, Node* localNode);


extern void __NodeStoreEx_handleNodeVersion(NodeStoreEx* this, Node* node);


// getters & setters
static inline NodeType NodeStoreEx_getStoreType(NodeStoreEx* this);


/**
 * This is the corresponding class for NodeStoreServers in userspace.
 */
struct NodeStoreEx
{
   App* app;

   RWLock rwLock;
   struct completion* newNodeAppeared; /* signalled when nodes are added (NULL without waiters) */

   NodeTree nodeTree;

   NodeOrGroup _rootOwner;

   NodeType storeType; // will be applied to nodes on addOrUpdate()
};

NodeType NodeStoreEx_getStoreType(NodeStoreEx* this)
{
   return this->storeType;
}

#endif /*NODESTOREEX_H_*/
