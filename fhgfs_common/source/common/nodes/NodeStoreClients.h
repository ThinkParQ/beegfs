#ifndef NODESTORECLIENTS_H_
#define NODESTORECLIENTS_H_

#include <common/threading/Mutex.h>
#include <common/threading/Condition.h>
#include <common/toolkit/Random.h>
#include <common/app/log/LogContext.h>
#include <common/nodes/Node.h>
#include <common/storage/StorageErrors.h>
#include <common/Common.h>
#include "AbstractNodeStore.h"


class NodeStoreClients : public AbstractNodeStore
{
   public:
      NodeStoreClients(bool channelsDirectDefault);

      virtual bool addOrUpdateNode(NodeHandle node) override;
      virtual bool addOrUpdateNodeEx(NodeHandle node, NumNodeID* outNodeNumID) override;
      bool updateLastHeartbeatT(NumNodeID numNodeID);
      virtual bool deleteNode(NumNodeID numNodeID);

      NodeHandle referenceNode(NumNodeID numNodeID);

      bool isNodeActive(NumNodeID numNodeID);
      virtual size_t getSize() override;

      virtual NodeHandle referenceFirstNode() override;
      virtual NodeHandle referenceNextNode(const NodeHandle& oldNode) override;

      virtual std::vector<NodeHandle> referenceAllNodes() override;

      void syncNodes(const std::vector<NodeHandle>& masterList, NumNodeIDList* outAddedIDs,
         NumNodeIDList* outRemovedIDs, bool updateExisting, Node* appLocalNode=NULL);


   protected:
      Mutex mutex;
      Condition newNodeCond; // set when a new node is added to the store (or undeleted)
      Random randGen; // must also be synchronized by mutex

      bool channelsDirectDefault; // for connpools, false to make all channels indirect by default

      NodeMap activeNodes;

   private:
      NumNodeID lastUsedNumID; // Note: is normally set in addOrUpdateNode

      NumNodeID generateNodeNumID(Node& node);
      NumNodeID retrieveNumIDFromStringID(std::string nodeID);
      bool checkNodeNumIDCollision(NumNodeID numID);
};

#endif /*NODESTORECLIENTS_H_*/
