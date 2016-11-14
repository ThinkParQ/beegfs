#include "GetStorageNodesWork.h"
#include <program/Program.h>

void GetStorageNodesWork::process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   std::vector<NodeHandle> storageNodesList;
   NumNodeIDList addedStorageNodes;
   NumNodeIDList removedStorageNodes;

   // get an updated list of metadata nodes
   if(NodesTk::downloadNodes(*mgmtdNode, NODETYPE_Storage, storageNodesList, false))
   {
      // sync the downloaded list with the node store
      storageNodes->syncNodes(storageNodesList, &addedStorageNodes, &removedStorageNodes, true);

      if(addedStorageNodes.size())
         log.log(Log_WARNING, "Nodes added (sync results): "
            + StringTk::uintToStr(addedStorageNodes.size())
            + " (Type: " + Node::nodeTypeToStr(NODETYPE_Storage) + ")");

      if(removedStorageNodes.size())
         log.log(Log_WARNING, "Nodes removed (sync results): "
            + StringTk::uintToStr(removedStorageNodes.size())
            + " (Type: " + Node::nodeTypeToStr(NODETYPE_Storage) + ")");

      // for each node added, get some general information
      for(NumNodeIDListIter iter = addedStorageNodes.begin(); iter != addedStorageNodes.end();
         iter++)
      {
         Work* work = new GetNodeInfoWork(*iter, NODETYPE_Storage);
         Program::getApp()->getWorkQueue()->addIndirectWork(work);
      }
   }
   else
   {
      log.log(Log_ERR, "Couldn't download storage server list from management daemon.");
   }

   UInt16List buddyGroupIDList;
   UInt16List primaryTargetIDList;
   UInt16List secondaryTargetIDList;

   // update the meta buddy groups
   if (NodesTk::downloadMirrorBuddyGroups(*mgmtdNode, NODETYPE_Storage, &buddyGroupIDList,
       &primaryTargetIDList, &secondaryTargetIDList, false) )
   {
      storageBuddyGroups->syncGroupsFromLists(buddyGroupIDList, primaryTargetIDList,
         secondaryTargetIDList, NumNodeID() );
   }
}
