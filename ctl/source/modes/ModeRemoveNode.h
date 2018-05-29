#ifndef MODEREMOVENODE_H_
#define MODEREMOVENODE_H_

#include <common/storage/StorageErrors.h>
#include <common/Common.h>
#include "Mode.h"


class ModeRemoveNode : public Mode
{
   public:
      ModeRemoveNode()
         : cfgNodeID("<undefined>"),
           cfgRemoveUnreachable(false),
           cfgReachabilityNumRetries(6), // (>= 1)
           cfgReachabilityRetryTimeoutMS(500),
           cfgForce(false)
      {}

      virtual int execute();

      static void printHelp();


   private:
      std::string cfgNodeID;
      bool cfgRemoveUnreachable;
      unsigned cfgReachabilityNumRetries; // (>= 1)
      unsigned cfgReachabilityRetryTimeoutMS;
      bool cfgForce;

      int removeSingleNode(NumNodeID nodeNumID, NodeType nodeType);
      int removeUnreachableNodes(NodeType nodeType);
      FhgfsOpsErr removeNodeComm(NumNodeID nodeNumID, NodeType nodeType);
      FhgfsOpsErr isPartOfMirrorBuddyGroup(Node& mgmtNode, NumNodeID nodeNumID, NodeType nodeType,
         bool* outIsPartOfMBG);
};


#endif /* MODEREMOVENODE_H_ */
