#ifndef NODESTORESERVERSEX_H_
#define NODESTORESERVERSEX_H_

#include <common/nodes/NodeStoreServers.h>

class NodeStoreServersEx : public NodeStoreServers
{
   public:
      NodeStoreServersEx(NodeType storeType) throw(InvalidConfigException);

      virtual bool addOrUpdateNode(std::shared_ptr<Node> node);
};

#endif /*NODESTORESERVERSEX_H_*/
