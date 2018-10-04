#include <program/Program.h>
#include "GetMetaNodesWork.h"

void GetMetaNodesWork::process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   std::vector<NodeHandle> metaNodesList;
   NumNodeIDList addedMetaNodes;
   NumNodeIDList removedMetaNodes;
   NumNodeID rootNodeID;
   bool rootIsBuddyMirrored;

   // get an updated list of metadata nodes
   if(NodesTk::downloadNodes(
      *mgmtdNode, NODETYPE_Meta, metaNodesList, false, &rootNodeID, &rootIsBuddyMirrored) )
   {
      // sync the downloaded list with the node store
      metaNodes->syncNodes(metaNodesList, &addedMetaNodes, &removedMetaNodes);

      Program::getApp()->getMetaRoot().setIfDefault(rootNodeID, rootIsBuddyMirrored);

      if(!addedMetaNodes.empty())
         log.log(Log_WARNING, "Meta nodes added (sync results): "
            + StringTk::uintToStr(addedMetaNodes.size()));

      if(!removedMetaNodes.empty())
         log.log(Log_WARNING, "Meta nodes removed (sync results): "
            + StringTk::uintToStr(removedMetaNodes.size()));

      // for each node added, get some general information
      for(NumNodeIDListIter iter = addedMetaNodes.begin(); iter != addedMetaNodes.end(); iter++)
      {
         Program::getApp()->getWorkQueue()->addIndirectWork(
            new GetNodeInfoWork(*iter, NODETYPE_Meta));
      }
   }
   else
   {
      log.log(Log_ERR, "Couldn't download metadata server list from management daemon.");
   }

   UInt16List buddyGroupIDList;
   UInt16List primaryTargetIDList;
   UInt16List secondaryTargetIDList;

   // update the meta buddy groups
   if (NodesTk::downloadMirrorBuddyGroups(*mgmtdNode, NODETYPE_Meta, &buddyGroupIDList,
       &primaryTargetIDList, &secondaryTargetIDList, false) )
   {
      metaBuddyGroups->syncGroupsFromLists(buddyGroupIDList, primaryTargetIDList,
         secondaryTargetIDList, NumNodeID() );
   }
}
