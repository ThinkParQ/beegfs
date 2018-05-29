#ifndef GETNODESWORK_H_
#define GETNODESWORK_H_

#include <common/components/worker/Work.h>
#include <common/nodes/MirrorBuddyGroupMapper.h>
#include <common/nodes/NodeType.h>
#include <common/nodes/NodeStoreServers.h>

class GetNodesWork : public Work
{
   public:
      GetNodesWork(std::shared_ptr<Node> mgmtdNode, NodeStoreServers *nodes, NodeType nodeType,
         MirrorBuddyGroupMapper* buddyGroupMapper, std::shared_ptr<Node> localNode)
          : mgmtdNode(std::move(mgmtdNode)),
            nodes(nodes),
            nodeType(nodeType),
            buddyGroupMapper(buddyGroupMapper),
            localNode(localNode)
      {}

      virtual void process(char* bufIn, unsigned bufInLen,
            char* bufOut, unsigned bufOutLen) override;

   private:
      std::shared_ptr<Node> mgmtdNode;
      NodeStoreServers* nodes;
      NodeType nodeType;
      MirrorBuddyGroupMapper* buddyGroupMapper;
      std::shared_ptr<Node> localNode;
};

#endif /*GETNODESWORK_H_*/
