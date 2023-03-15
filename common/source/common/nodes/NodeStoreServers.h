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
      NodeStoreServers(NodeType storeType, bool channelsDirectDefault);

      virtual NodeStoreResult addOrUpdateNode(NodeHandle node) override;
      virtual NodeStoreResult addOrUpdateNodeEx(NodeHandle node, NumNodeID* outNodeNumID) override;
      virtual bool deleteNode(NumNodeID id);

      NodeHandle referenceNode(NumNodeID id) const;
      NodeHandle referenceNodeByTargetID(uint16_t targetID, TargetMapper* targetMapper,
         FhgfsOpsErr* outErr=NULL) const;

      virtual NodeHandle referenceFirstNode() const override;

      virtual std::vector<NodeHandle> referenceAllNodes() const override;

      bool isNodeActive(NumNodeID id) const;
      virtual size_t getSize() const override;

      bool waitForFirstNode(int waitTimeoutMS) const;

      void syncNodes(const std::vector<NodeHandle>& masterList, NumNodeIDList* outAddedNumIDs,
         NumNodeIDList* outRemovedNumIDs, Node* appLocalNode=NULL);

      void attachCapacityPools(NodeCapacityPools* capacityPools);
      void attachTargetMapper(TargetMapper* targetMapper);
      void attachStateStore(TargetStateStore* stateStore);

      std::string getNodeIDWithTypeStr(NumNodeID numID) const;
      std::string getTypedNodeID(NumNodeID numID) const;

   protected:
      mutable Mutex mutex;
      mutable Condition newNodeCond; // set when a new node is added to the store (or undeleted)
      std::shared_ptr<Node> localNode;

      bool channelsDirectDefault; // for connpools, false to make all channels indirect by default

      NodeMap activeNodes; // key is numeric node id

      NodeCapacityPools* capacityPools; // optional for auto remove (may be NULL)
      TargetMapper* targetMapper; // optional for auto remove (may be NULL)
      TargetStateStore* stateStore; // optional for auto remove (may be NULL)

      NodeStoreResult addOrUpdateNodeUnlocked(NodeHandle node, NumNodeID* outNodeNumID);

      virtual NumNodeID generateID(Node& node) const
      {
         return {};
      }

      NumNodeID retrieveNumIDFromStringID(std::string nodeID) const;

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

         if (localNode)
            activeNodes.insert({localNode->getNumID(), localNode});
      }
};

#endif /*NODESTORESERVERS_H_*/
