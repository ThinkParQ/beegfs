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
      storageNodes->syncNodes(storageNodesList, &addedStorageNodes, &removedStorageNodes);

      if(!addedStorageNodes.empty())
         log.log(Log_WARNING, "Storage nodes added (sync results): "
            + StringTk::uintToStr(addedStorageNodes.size()));

      if(!removedStorageNodes.empty())
         log.log(Log_WARNING, "Storage nodes removed (sync results): "
            + StringTk::uintToStr(removedStorageNodes.size()));

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
