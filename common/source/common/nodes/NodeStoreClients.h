#ifndef NODESTORECLIENTS_H_
#define NODESTORECLIENTS_H_

#include <common/threading/Mutex.h>
#include <common/threading/Condition.h>
#include <common/app/log/LogContext.h>
#include <common/nodes/Node.h>
#include <common/storage/StorageErrors.h>
#include <common/Common.h>
#include "AbstractNodeStore.h"

class NodeStoreClients : public AbstractNodeStore
{
   public:
      NodeStoreClients();

      virtual NodeStoreResult addOrUpdateNode(NodeHandle node) override;
      virtual NodeStoreResult addOrUpdateNodeEx(NodeHandle node, NumNodeID* outNodeNumID) override;
      virtual bool deleteNode(NumNodeID nodeNumID);

      NodeHandle referenceNode(NumNodeID numNodeID) const;

      bool isNodeActive(NumNodeID nodeNumID) const;
      virtual size_t getSize() const override;

      virtual NodeHandle referenceFirstNode() const override;

      virtual std::vector<NodeHandle> referenceAllNodes() const override;

      void syncNodes(const std::vector<NodeHandle>& masterList, NumNodeIDList* outAddedIDs,
         NumNodeIDList* outRemovedIDs);


   protected:
      mutable Mutex mutex;
      Condition newNodeCond; // set when a new node is added to the store (or undeleted)

      NodeMap activeNodes;

      virtual NumNodeID generateID(Node& node)
      {
         return {};
      }
};

#endif /*NODESTORECLIENTS_H_*/
