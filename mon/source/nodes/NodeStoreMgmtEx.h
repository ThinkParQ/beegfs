#ifndef NODESTOREMGMTDEX_H_
#define NODESTOREMGMTDEX_H_

#include <common/nodes/NodeStore.h>
#include <nodes/MgmtNodeEx.h>

class NodeStoreMgmtEx : public NodeStoreServers
{
   public:
      NodeStoreMgmtEx();

      virtual NodeStoreResult addOrUpdateNodeEx(std::shared_ptr<Node> node, NumNodeID* outNodeNumID) override;
};

#endif /*NODESTOREMGMTDEX_H_*/
