#include <common/net/message/nodes/RemoveNodeRespMsg.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "RemoveNodeMsgEx.h"


bool RemoveNodeMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();

   LOG_DBG(GENERAL, SPAM, "Removing node.", getNodeNumID());

   bool nodeRemoved = false;
   NodeHandle node;

   if (app->isShuttingDown())
   {
      if (!wantsAck())
         ctx.sendResponse(GenericResponseMsg(GenericRespMsgCode_TRYAGAIN, "Mgmtd shutting down."));
      return true;
   }

   // delete node based on type

   if(getNodeType() == NODETYPE_Client)
   {
      NodeStoreClients* nodes = app->getClientNodes();
      node = nodes->referenceNode(getNodeNumID());
      nodeRemoved = nodes->deleteNode(getNodeNumID());
   }
   else
   { // not a client => server node
      NodeStoreServers* nodes = app->getServerStoreFromType(getNodeType() );
      if(!nodes)
      { // invalid node type
         LOG(GENERAL, ERR, "Invalid node type.",
               ("Node Type", getNodeType()),
               ("Sender", ctx.peerName()),
               ("NodeID", getNodeNumID())
            );
      }
      else
      { // found the corresponding server store => delete by ID

         UInt16List targetIDs;
         TargetMapper* mapper = app->getTargetMapper();
         mapper->getTargetsByNode(getNodeNumID(), targetIDs);

         node = nodes->referenceNode(getNodeNumID());
         nodeRemoved = nodes->deleteNode(getNodeNumID());

         if(app->getConfig()->getQuotaEnableEnforcement() )
         {
            for(UInt16ListIter iter = targetIDs.begin(); iter != targetIDs.end(); iter++)
               app->getQuotaManager()->removeTargetFromQuotaDataStores(*iter);
         }
      }
   }

   // log message and notification of other nodes

   if(nodeRemoved)
   {
      LOG(GENERAL, WARNING, "Node removed.", ("node", node->getNodeIDWithTypeStr()));

      if(getNodeType() != NODETYPE_Client) // don't print stats for every client unmount
         LOG(GENERAL, NOTICE, "Number of nodes: ",
               ("meta", app->getMetaNodes()->getSize()),
               ("storage", app->getStorageNodes()->getSize()),
               ("client", app->getClientNodes()->getSize()),
               ("mgmt", app->getMgmtNodes()->getSize()));

      // force update of capacity pools (especially to update buddy mirror capacity pool)
      app->getInternodeSyncer()->setForcePoolsUpdate();

      app->getHeartbeatMgr()->notifyAsyncRemovedNode(getNodeNumID(), getNodeType());
   }

   if(!acknowledge(ctx) )
      ctx.sendResponse(
            RemoveNodeRespMsg(nodeRemoved ? FhgfsOpsErr_SUCCESS : FhgfsOpsErr_UNKNOWNNODE) );

   return true;
}

