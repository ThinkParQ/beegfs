#ifndef NODESTORESTORAGEEX_H_
#define NODESTORESTORAGEEX_H_

#include <common/nodes/NodeStore.h>

class NodeStoreStorageEx : public NodeStoreServers
{
   public:
      NodeStoreStorageEx();

      virtual NodeStoreResult addOrUpdateNodeEx(std::shared_ptr<Node> receivedNode,
            NumNodeID* outNodeNumID) override;
};

#endif /*NODESTORESTORAGEEX_H_*/
