#ifndef NODESTORESERVERS_H_
#define NODESTORESERVERS_H_

#include <common/app/log/LogContext.h>
#include <common/threading/Mutex.h>
#include <common/threading/Condition.h>
#include <common/toolkit/Random.h>
#include <common/nodes/Node.h>
#include <common/nodes/NodeCapacityPools.h>
#include <common/nodes/TargetMapper.h>
#include <common/storage/StorageErrors.h>
#include <common/Common.h>
#include "AbstractNodeStore.h"


class NodeStoreServers : public AbstractNodeStore
{
   public:
      NodeStoreServers(NodeType storeType, bool channelsDirectDefault)
         throw(InvalidConfigException);

      virtual bool addOrUpdateNode(NodeHandle node);
      virtual bool addOrUpdateNodeEx(NodeHandle node, NumNodeID* outNodeNumID);
      bool updateLastHeartbeatT(NumNodeID id);
      virtual bool deleteNode(NumNodeID id);

      NodeHandle referenceNode(NumNodeID id);
      NodeHandle referenceRootNode(MirrorBuddyGroupMapper* metaBuddyGroupMapper);
      NodeHandle referenceNodeByTargetID(uint16_t targetID, TargetMapper* targetMapper,
         FhgfsOpsErr* outErr=NULL);
      NodeHandle referenceNodeByStringID(std::string nodeStrID);

      virtual NodeHandle referenceFirstNode();
      virtual NodeHandle referenceNextNode(const NodeHandle& oldNode);

      virtual std::vector<NodeHandle> referenceAllNodes();

      bool isNodeActive(NumNodeID id);
      virtual size_t getSize();

      NumNodeID getRootNodeNumID();
      bool getRootIsBuddyMirrored();
      virtual bool setRootNodeNumID(NumNodeID id, bool ignoreExistingRoot, bool isBuddyMirrored);

      bool waitForFirstNode(int waitTimeoutMS);

      void syncNodes(const std::vector<NodeHandle>& masterList, NumNodeIDList* outAddedIDs,
         NumNodeIDList* outRemovedIDs, bool updateExisting, Node* appLocalNode=NULL);

      void attachCapacityPools(NodeCapacityPools* capacityPools);
      void attachTargetMapper(TargetMapper* targetMapper);
      void attachStateStore(TargetStateStore* stateStore);

      std::string getNodeIDWithTypeStr(NumNodeID numID);
      std::string getTypedNodeID(NumNodeID numID);

   protected:
      Mutex mutex;
      Condition newNodeCond; // set when a new node is added to the store (or undeleted)
      std::shared_ptr<Node> localNode;
      NumNodeID rootNodeID;
      bool rootIsBuddyMirrored;
      Random randGen; // must also be synchronized by mutex

      bool channelsDirectDefault; // for connpools, false to make all channels indirect by default

      NodeMap activeNodes; // key is numeric node id

      NodeCapacityPools* capacityPools; // optional for auto remove (may be NULL)
      TargetMapper* targetMapper; // optional for auto remove (may be NULL)
      TargetStateStore* stateStore; // optional for auto remove (may be NULL)


      bool addOrUpdateNodeUnlocked(NodeHandle node, NumNodeID* outNodeNumID);
      bool addOrUpdateNodePrecheck(Node& node);


   private:
      NumNodeID generateNodeNumID(Node& node);
      NumNodeID retrieveNumIDFromStringID(std::string nodeID);
      bool checkNodeNumIDCollision(NumNodeID numID);

      void handleNodeVersion(Node& node);

   public:
      // getters & setters

      /**
       * Note: this is supposed to be called before the multi-threading starts => no locking.
       *
       * @param localNode node object is owned by the app, not by this store.
       */
      void setLocalNode(const std::shared_ptr<Node>& localNode)
      {
         this->localNode = localNode;

         if(capacityPools && localNode)
            capacityPools->addIfNotExists(localNode->getNumID().val(), CapacityPool_LOW);
      }
};

#endif /*NODESTORESERVERS_H_*/
