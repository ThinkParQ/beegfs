#ifndef NODESTOREMETAEX_H_
#define NODESTOREMETAEX_H_

#include <common/nodes/NodeStore.h>

class NodeStoreMetaEx : public NodeStoreServers
{
   public:
      NodeStoreMetaEx();

      virtual NodeStoreResult addOrUpdateNodeEx(std::shared_ptr<Node> receivedNode,
            NumNodeID* outNodeNumID) override;

};

#endif /*NODESTOREMETAEX_H_*/
