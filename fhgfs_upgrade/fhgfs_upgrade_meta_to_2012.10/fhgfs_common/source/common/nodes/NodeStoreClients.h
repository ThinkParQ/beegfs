#ifndef NODESTORECLIENTS_H_
#define NODESTORECLIENTS_H_

#include <common/threading/Mutex.h>
#include <common/threading/Condition.h>
#include <common/toolkit/ObjectReferencer.h>
#include <common/toolkit/Random.h>
#include <common/app/log/LogContext.h>
#include <common/nodes/Node.h>
#include <common/storage/StorageErrors.h>
#include <common/Common.h>
#include "AbstractNodeStore.h"


class NodeStoreClients : public AbstractNodeStore
{
   // type definitions

   typedef ObjectReferencer<Node*> NodeReferencer;
   typedef std::map<std::string, NodeReferencer*> NodeMap;
   typedef NodeMap::iterator NodeMapIter;
   typedef NodeMap::value_type NodeMapVal;


   public:
      NodeStoreClients(bool channelsDirectDefault) throw(InvalidConfigException);
      virtual ~NodeStoreClients();

      virtual bool addOrUpdateNode(Node** node);
      bool updateLastHeartbeatT(std::string nodeID);
      virtual bool deleteNode(std::string nodeID);

      Node* referenceNode(std::string id);
      void releaseNode(Node** node);

      bool isNodeActive(std::string nodeID);
      virtual size_t getSize();

      virtual Node* referenceFirstNode();
      virtual Node* referenceNextNodeAndReleaseOld(Node* oldNode);
      Node* referenceNextNode(std::string nodeID);

      virtual void referenceAllNodes(NodeList* outList);
      virtual void releaseAllNodes(NodeList* list);

      void syncNodes(NodeList* masterList, StringList* outAddedIDs, StringList* outRemovedIDs,
         bool updateExisting, Node* appLocalNode=NULL);


   protected:
      Mutex mutex;
      Condition newNodeCond; // set when a new node is added to the store (or undeleted)
      Node* localNode;
      Random randGen; // must also be synchronized by mutex

      bool channelsDirectDefault; // for connpools, false to make all channels indirect by default

      NodeMap activeNodes;
      NodeMap deletedNodes;


   private:
      void releaseNodeUnlocked(Node** node);


   public:
      // getters & setters

      /**
       * Note: this is supposed to be called before the multi-threading starts => no mutex.
       *
       * @param localNode localNode object is owned by the app, not by this store.
       */
      void setLocalNode(Node* localNode)
      {
         this->localNode = localNode;
      }

};

#endif /*NODESTORECLIENTS_H_*/
