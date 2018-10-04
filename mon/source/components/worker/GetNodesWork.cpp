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
      nodes->syncNodes(nodesList, &addedNodes, &removedNodes, localNode.get());

      if (!addedNodes.empty())
         LOG(GENERAL, WARNING, "Nodes added.", ("addedNodes", addedNodes.size()), nodeType);

      if (!removedNodes.empty())
         LOG(GENERAL, WARNING, "Nodes removed.", ("removedNodes", removedNodes.size()), nodeType);
   }
   else
   {
      LOG(GENERAL, ERR, "Couldn't download server list from management daemon.", nodeType);
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
