#ifndef NODESTK_H_
#define NODESTK_H_

#include <nodes/NodeStoreEx.h>
#include <common/Common.h>
#include <common/toolkit/list/StrCpyListIter.h>
#include <common/toolkit/list/UInt8List.h>


extern bool NodesTk_downloadNodes(App* app, Node* sourceNode, NodeType nodeType,
   NodeList* outNodeList, NumNodeID* outRootNodeID, bool* outRootIsBuddyMirrored);
extern bool NodesTk_downloadTargetMappings(App* app, Node* sourceNode,
   UInt16List* outTargetIDs, NumNodeIDList* outNodeIDs);
extern bool NodesTk_downloadMirrorBuddyGroups(App* app, Node* sourceNode, NodeType nodeType,
   UInt16List* outBuddyGroupIDs, UInt16List* outPrimaryTargetIDs,
   UInt16List* outSecondaryTargetIDs);
extern bool NodesTk_downloadStatesAndBuddyGroups(App* app, Node* sourceNode,
   NodeType nodeType, UInt16List* outBuddyGroupIDs, UInt16List* outPrimaryTargetIDs,
   UInt16List* outSecondaryTargetIDs, UInt16List* outTargetIDs,
   UInt8List* outTargetReachabilityStates, UInt8List* outTargetConsistencyStates);
extern unsigned NodesTk_dropAllConnsByStore(NodeStoreEx* nodes);


#endif /* NODESTK_H_ */
