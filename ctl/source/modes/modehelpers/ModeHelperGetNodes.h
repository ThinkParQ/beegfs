#ifndef MODEHELPERGETNODES_H_
#define MODEHELPERGETNODES_H_

#include <common/Common.h>
#include <common/nodes/NodeStore.h>

class ModeHelperGetNodes
{
   public:
      static void checkReachability(NodeType nodeType, const std::vector<NodeHandle>& nodes,
         std::set<NumNodeID>& outUnreachableNodes, unsigned numRetries, unsigned retryTimeoutMS);
      static void pingNodes(NodeType nodeType, const std::vector<NodeHandle>& nodes,
         unsigned numPingRetries);
      static void connTest(NodeType nodeType, const std::vector<NodeHandle>& nodes,
         unsigned numConns);


   private:
      ModeHelperGetNodes() {}
};

#endif /* MODEHELPERGETNODES_H_ */
