#include <common/net/message/nodes/RemoveNodeRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "RemoveNodeMsgEx.h"


bool RemoveNodeMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();

   LOG_DBG(DEBUG, "Received a RemoveNodeMsg.", ctx.peerName());

   LOG_DBG(SPAM, "Removing node.", getNodeNumID());

   if (getNodeType() == NODETYPE_Storage)
   {
      NodeStoreServersEx* nodes = app->getStorageNodes();
      auto node = nodes->referenceNode(getNodeNumID());
      bool delRes = nodes->deleteNode(getNodeNumID());

      // log
      if (delRes)
      {
         LOG(WARNING, "Node removed.", as("node", node->getNodeIDWithTypeStr()));
         LOG(WARNING, "Number of nodes in the system:",
               as("meta", app->getMetaNodes()->getSize()),
               as("storage", app->getStorageNodes()->getSize()));
      }
   }

   if (!acknowledge(ctx))
      ctx.sendResponse(RemoveNodeRespMsg(0));

   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), StorageOpCounter_REMOVENODE,
      getMsgHeaderUserID() );

   return true;
}

