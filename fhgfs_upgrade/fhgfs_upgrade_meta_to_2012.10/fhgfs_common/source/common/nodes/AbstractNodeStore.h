#ifndef ABSTRACTNODESTORE_H_
#define ABSTRACTNODESTORE_H_

#include <common/Common.h>


class AbstractNodeStore
{
   public:
      virtual bool addOrUpdateNode(Node** node) = 0;

      virtual Node* referenceFirstNode() = 0;
      virtual Node* referenceNextNodeAndReleaseOld(Node* oldNode) = 0;

      virtual void referenceAllNodes(NodeList* outList) = 0;
      virtual void releaseAllNodes(NodeList* list) = 0;

      virtual size_t getSize() = 0;


   protected:
      AbstractNodeStore(NodeType storeType) : storeType(storeType) {}
      virtual ~AbstractNodeStore() {}

      NodeType storeType; // will be applied to all contained nodes on addOrUpdate()


   public:
      // getters & setters

      NodeType getStoreType()
      {
         return storeType;
      }
};

#endif /* ABSTRACTNODESTORE_H_ */
