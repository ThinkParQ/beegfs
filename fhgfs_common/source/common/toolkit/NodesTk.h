#ifndef NODESTK_H_
#define NODESTK_H_

#include <common/components/AbstractDatagramListener.h>
#include <common/nodes/AbstractNodeStore.h>
#include <common/nodes/NodeStoreServers.h>
#include <common/nodes/TargetStateStore.h>
#include <common/Common.h>

class NodesTk
{
   public:
      static bool waitForMgmtHeartbeat(PThread* currentThread, AbstractDatagramListener* dgramLis,
         NodeStoreServers* mgmtNodes, std::string hostname, unsigned short portUDP,
         unsigned timeoutMS=0);
      static bool downloadNodes(Node& sourceNode, NodeType nodeType,
         std::vector<NodeHandle>& outNodes, bool silenceLog, NumNodeID* outRootNumID=NULL,
         bool* outRootIsBuddyMirrored=NULL);
      static bool downloadTargetMappings(Node& sourceNode, UInt16List* outTargetIDs,
         NumNodeIDList* outNodeIDs, bool silenceLog);
      static bool downloadMirrorBuddyGroups(Node& sourceNode, NodeType nodeType,
         UInt16List* outBuddyGroupIDs, UInt16List* outPrimaryTargetIDs,
         UInt16List* outSecondaryTargetIDs, bool silenceLog);
      static bool downloadTargetStates(Node& sourceNode, NodeType nodeType, UInt16List* outTargetIDs,
         UInt8List* outReachabilityStates, UInt8List* outConsistencyStates, bool silenceLog);
      static bool downloadStatesAndBuddyGroups(Node& sourceNode, NodeType nodeType,
         UInt16List* outBuddyGroupIDs, UInt16List* outPrimaryTargetIDs,
         UInt16List* outSecondaryTargetIDs, UInt16List* outTargetIDs,
         UInt8List* outTargetReachabilityStates, UInt8List* outTargetConsistencyStates,
         bool silenceLog);


      static void moveNodesFromListToStore(std::vector<NodeHandle>& nodes,
         AbstractNodeStore* nodeStore);
      static void applyLocalNicCapsToList(Node& localNode, const std::vector<NodeHandle>& nodes);

      static unsigned getRetryDelayMS(int elapsedMS);

   private:
      NodesTk() {}
};


#endif /*NODESTK_H_*/
