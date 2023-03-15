#include "NodeStoreStorageEx.h"

#include <common/app/log/Logger.h>
#include <nodes/StorageNodeEx.h>

NodeStoreStorageEx::NodeStoreStorageEx() :
   NodeStoreServers(NODETYPE_Storage, false)
{}

NodeStoreResult NodeStoreStorageEx::addOrUpdateNodeEx(std::shared_ptr<Node> receivedNode,
      NumNodeID* outNodeNumID)
{
   // sanity check: don't allow nodeNumID==0 (only mgmtd allows this)
   if (!receivedNode->getNumID())
      return NodeStoreResult::Error;

   std::shared_ptr<StorageNodeEx> newNode;
   auto storedNode =
         std::static_pointer_cast<StorageNodeEx>(referenceNode(receivedNode->getNumID()));
   if (!storedNode)
   {
      // new node, create StorageNodeEx object with the parameters of the received node info
      newNode = std::make_shared<StorageNodeEx>(receivedNode);
      LOG(GENERAL, DEBUG, "Received new storage node.",
            ("nodeNumID", receivedNode->getNumID().val()));
   }
   else
   {
      // already stored node, create StorageNodeEx object with the parameters of the
      // received node info and keep the internal data
      newNode = std::make_shared<StorageNodeEx>(receivedNode, storedNode);
      LOG(GENERAL, DEBUG, "Received update for storage node.",
            ("nodeNumID", receivedNode->getNumID().val()));
   }

   const std::lock_guard<Mutex> lock(mutex);
   return addOrUpdateNodeUnlocked(std::move(newNode), outNodeNumID);
}
