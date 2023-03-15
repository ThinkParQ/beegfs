#include "NodeStoreMetaEx.h"

#include <common/app/log/Logger.h>
#include <nodes/MetaNodeEx.h>

NodeStoreMetaEx::NodeStoreMetaEx() :
      NodeStoreServers(NODETYPE_Meta, false)
{}

NodeStoreResult NodeStoreMetaEx::addOrUpdateNodeEx(std::shared_ptr<Node> receivedNode,
      NumNodeID* outNodeNumID)
{
   // sanity check: don't allow nodeNumID==0 (only mgmtd allows this)
   if (!receivedNode->getNumID())
      return NodeStoreResult::Error;

   std::shared_ptr<MetaNodeEx> newNode;
   auto storedNode =
         std::static_pointer_cast<MetaNodeEx>(referenceNode(receivedNode->getNumID()));
   if (!storedNode)
   {
      // new node, create StorageNodeEx object with the parameters of the received node info
      newNode = std::make_shared<MetaNodeEx>(receivedNode);
      LOG(GENERAL, DEBUG, "Received new meta node.",
            ("nodeNumID", receivedNode->getNumID().val()));
   }
   else
   {
      // already stored node, create StorageNodeEx object with the parameters of the
      // received node info and keep the internal data
      newNode = std::make_shared<MetaNodeEx>(receivedNode, storedNode);
      LOG(GENERAL, DEBUG, "Received update for meta node.",
            ("nodeNumID", receivedNode->getNumID().val()));
   }

   const std::lock_guard<Mutex> lock(mutex);
   return addOrUpdateNodeUnlocked(std::move(newNode), nullptr);
}
