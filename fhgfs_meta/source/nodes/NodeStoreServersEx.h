#ifndef NODESTOREEXSERVERS_H_
#define NODESTOREEXSERVERS_H_

#include <common/nodes/NodeStoreServers.h>


class NodeStoreServersEx : public NodeStoreServers
{
   public:
      NodeStoreServersEx(NodeType storeType) throw(InvalidConfigException);

      virtual bool addOrUpdateNode(std::shared_ptr<Node> node);

      bool gotRoot();
};

#endif /*NODESTOREEXSERVERS_H_*/
