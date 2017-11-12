#include "GetNodesWork.h"

#include <common/toolkit/NodesTk.h>

void GetNodesWork::process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   std::vector<std::shared_ptr<Node>> nodesList;
   std::list<NumNodeID> addedNodes;
   std::list<NumNodeID> removedNodes;



   if (NodesTk::downloadNodes(*mgmtdNode, nodeType, nodesList, false))
   {
      // sync the downloaded list with the node store
      nodes->syncNodes(nodesList, &addedNodes, &removedNodes, false, localNode.get());

      if (addedNodes.size())
         LOG(GENERAL, WARNING, "Nodes added.", as("addedNodes", addedNodes.size()),
               as("NodeType", Node::nodeTypeToStr(nodeType)));

      if (removedNodes.size())
         LOG(GENERAL, WARNING, "Nodes removed.", as("removedNodes", removedNodes.size()),
               as("NodeType", Node::nodeTypeToStr(nodeType)));
   }
   else
   {
      LOG(GENERAL, ERR, "Couldn't download server list from management daemon.",
            as("nodeType", Node::nodeTypeToStr(nodeType)));
   }

   std::list<uint16_t> buddyGroupIDList;
   std::list<uint16_t> primaryTargetIDList;
   std::list<uint16_t> secondaryTargetIDList;

   // update the storage buddy groups
   if (NodesTk::downloadMirrorBuddyGroups(*mgmtdNode, nodeType, &buddyGroupIDList,
       &primaryTargetIDList, &secondaryTargetIDList, false) )
   {
      buddyGroupMapper->syncGroupsFromLists(buddyGroupIDList, primaryTargetIDList,
            secondaryTargetIDList, NumNodeID());
   }
}
