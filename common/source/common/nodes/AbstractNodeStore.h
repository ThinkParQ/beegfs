#ifndef ABSTRACTNODESTORE_H_
#define ABSTRACTNODESTORE_H_

#include <common/Common.h>
#include <common/nodes/Node.h>
#include <common/toolkit/ObjectReferencer.h>


class AbstractNodeStore
{
   protected:
      typedef std::map<NumNodeID, std::shared_ptr<Node>> NodeMap;

   public:
      virtual bool addOrUpdateNode(NodeHandle node) = 0;
      virtual bool addOrUpdateNodeEx(NodeHandle node, NumNodeID* outNodeNumID) = 0;

      virtual NodeHandle referenceFirstNode() const = 0;

      virtual std::vector<NodeHandle> referenceAllNodes() const = 0;

      virtual size_t getSize() const = 0;


   protected:
      AbstractNodeStore(NodeType storeType) : storeType(storeType) {}
      virtual ~AbstractNodeStore() {}

      AbstractNodeStore(const AbstractNodeStore&) = delete;
      AbstractNodeStore(AbstractNodeStore&&) = delete;
      AbstractNodeStore& operator=(const AbstractNodeStore&) = delete;
      AbstractNodeStore& operator=(AbstractNodeStore&&) = delete;

      NodeType storeType; // will be applied to all contained nodes on addOrUpdate()


   public:
      // getters & setters

      NodeType getStoreType() const
      {
         return storeType;
      }
};

#endif /* ABSTRACTNODESTORE_H_ */
