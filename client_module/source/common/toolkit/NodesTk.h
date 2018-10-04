#ifndef NODESTK_H_
#define NODESTK_H_

#include <nodes/NodeStoreEx.h>
#include <common/Common.h>
#include <common/toolkit/list/StrCpyListIter.h>
#include <common/toolkit/list/UInt8List.h>


extern bool NodesTk_downloadNodes(App* app, Node* sourceNode, NodeType nodeType,
   NodeList* outNodeList, NumNodeID* outRootNodeID, bool* outRootIsBuddyMirrored);
extern bool NodesTk_downloadTargetMappings(App* app, Node* sourceNode, struct list_head* mappings);
extern bool NodesTk_downloadStatesAndBuddyGroups(App* app, Node* sourceNode,
   NodeType nodeType, struct list_head* groups, struct list_head* states);
extern unsigned NodesTk_dropAllConnsByStore(NodeStoreEx* nodes);


#endif /* NODESTK_H_ */
