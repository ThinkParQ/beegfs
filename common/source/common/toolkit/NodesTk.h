#ifndef NODESTK_H_
#define NODESTK_H_

#include <common/components/AbstractDatagramListener.h>
#include <common/nodes/AbstractNodeStore.h>
#include <common/nodes/MirrorBuddyGroup.h>
#include <common/nodes/NodeStoreServers.h>
#include <common/nodes/TargetStateStore.h>
#include <common/Common.h>

class NodesTk
{
   public:
      static bool waitForMgmtHeartbeat(PThread* currentThread, AbstractDatagramListener* dgramLis,
         const NodeStoreServers* mgmtNodes, std::string hostname, unsigned short portUDP,
         unsigned timeoutMS, unsigned nameResolutionRetries);
      static std::shared_ptr<Node> downloadNodeInfo(const std::string& hostname,
         unsigned short port, uint64_t connAuthHash, AbstractNetMessageFactory* netMessageFactory,
         NodeType nodeType, int timeoutMS);
      static bool downloadNodes(Node& sourceNode, NodeType nodeType,
         std::vector<NodeHandle>& outNodes, bool silenceLog, NumNodeID* outRootNumID=NULL,
         bool* outRootIsBuddyMirrored=NULL);
      static std::pair<bool, TargetMap> downloadTargetMappings(const Node& sourceNode,
         bool silenceLog);
      static bool downloadMirrorBuddyGroups(const Node& sourceNode, NodeType nodeType,
         UInt16List* outBuddyGroupIDs, UInt16List* outPrimaryTargetIDs,
         UInt16List* outSecondaryTargetIDs, bool silenceLog);
      static bool downloadTargetStates(Node& sourceNode, NodeType nodeType, UInt16List* outTargetIDs,
         UInt8List* outReachabilityStates, UInt8List* outConsistencyStates, bool silenceLog);
      static bool downloadStatesAndBuddyGroups(Node& sourceNode, NodeType nodeType,
         MirrorBuddyGroupMap& outBuddyGroups, TargetStateMap& outStates,
         bool silenceLog);
      static bool downloadStoragePools(Node& sourceNode, StoragePoolPtrVec& outStoragePools,
         bool silenceLog);

      static void moveNodesFromListToStore(std::vector<NodeHandle>& nodes,
         AbstractNodeStore* nodeStore);
      static void applyLocalNicListToList(Node& localNode, const std::vector<NodeHandle>& nodes);
      static void applyLocalNicListToList(const NicAddressList& localNicList,
         const NicListCapabilities& localNicCaps, const std::vector<NodeHandle>& nodes);

      static unsigned getRetryDelayMS(int elapsedMS);

   private:
      NodesTk() {}
};


#endif /*NODESTK_H_*/
