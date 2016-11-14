#ifndef GETSTORAGENODESWORK_H_
#define GETSTORAGENODESWORK_H_

/*
 * GetStorageNodesWork is a work packet to download the storage nodes from the mgmtd and fetch some
 * general information about the nodes
 */

#include <common/components/worker/Work.h>
#include <common/net/message/nodes/GetNodesMsg.h>
#include <common/net/message/nodes/GetNodesRespMsg.h>
#include <common/net/message/NetMessage.h>
#include <nodes/NodeStoreStorageEx.h>
#include <components/worker/GetNodeInfoWork.h>
#include <app/App.h>

class GetStorageNodesWork: public Work
{
   public:
      /*
       * @param mgmtdNode Pointer to the management node
       * @param storageNodes The global storage server node store
       */
      GetStorageNodesWork(NodeHandle mgmtdNode, NodeStoreStorageEx* storageNodes,
         MirrorBuddyGroupMapper* storageBuddyGroups)
       : mgmtdNode(std::move(mgmtdNode)),
         storageNodes(storageNodes),
         storageBuddyGroups(storageBuddyGroups),
         log("GetStorageNodes Work")
      { }

      virtual ~GetStorageNodesWork() {}

      void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen);

   private:
      NodeHandle mgmtdNode;
      NodeStoreStorageEx* storageNodes;
      MirrorBuddyGroupMapper* storageBuddyGroups;
      LogContext log;
};

#endif /*GETSTORAGENODESWORK_H_*/
