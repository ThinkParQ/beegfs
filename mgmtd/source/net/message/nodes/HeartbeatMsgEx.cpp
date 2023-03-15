#include <common/net/sock/NetworkInterfaceCard.h>
#include <common/nodes/NodeStoreClients.h>
#include <common/nodes/NodeStoreServers.h>
#include <net/message/nodes/RegisterNodeMsgEx.h>
#include <program/Program.h>
#include "HeartbeatMsgEx.h"


bool HeartbeatMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();

   if (app->isShuttingDown())
      return true;

   NodeCapacityPools* metaCapacityPools = app->getMetaCapacityPools();
   HeartbeatManager* heartbeatMgr = app->getHeartbeatMgr();

   bool isNodeNew;

   NodeType nodeType = getNodeType();
   std::string nodeID(getNodeID() );

   NicAddressList& nicList = getNicList();

   LOG(GENERAL, DEBUG, "Heartbeat node", ("nodeType", nodeType),
      ("nodeId", nodeID));

   // check for empty nodeID; (sanity check, should never fail)
   if(unlikely(nodeID.empty() ) )
   {
      LOG(GENERAL, WARNING, "Rejecting heartbeat of node with empty long ID.",
          ctx.peerName(), nodeType);

      return false;
   }


   if(nodeType == NODETYPE_Client)
   { // this is a client heartbeat
      NodeStoreClients* clients = app->getClientNodes();

      if (getPortUDP() <= 0 || getPortTCP() != 0) {
         LOG(GENERAL, WARNING, "Unable to register client because of wrong port.",
             ("PortTCP",  getPortTCP()), ("addr", ctx.peerName()), 
             (("nodeType"), nodeType));

         return false;
      }

      // construct node

      auto node = RegisterNodeMsgEx::constructNode(nodeType, nodeID, getNodeNumID(), getPortUDP(),
            getPortTCP(), nicList);

      // add node to store (or update it)

      isNodeNew = (clients->addOrUpdateNode(std::move(node)) == NodeStoreResult::Added);
   }
   else
   { // this is a server heartbeat

      /* only accept new servers if nodeNumID is set
         (otherwise RegisterNodeMsg would need to be called first) */

      if (getPortUDP() <= 0 || getPortTCP() == 0) {
         LOG(GENERAL, WARNING, "Unable to register server because of wrong port.",
               ("PortTCP",  getPortTCP()), ("addr", ctx.peerName()), 
               (("nodeType"), nodeType));

         return false;
      }

      if(!getNodeNumID() )
      { /* shouldn't happen: this server would need to register first to get a nodeNumID assigned */

         LOG(GENERAL, WARNING, "Rejecting heartbeat of node without numeric ID.", nodeID, nodeType);

         return false;
      }

      // get the corresponding node store for this node type

      NodeStoreServers* servers = app->getServerStoreFromType(nodeType);
      if(unlikely(!servers) )
      {
         LOG(GENERAL, ERR, "Invalid node type.",
               ("Node Type", getNodeType()),
               ("Sender", ctx.peerName()),
               ("NodeID", getNodeNumID()),
               ("Port (UDP)", getPortUDP()),
               ("Port (TCP)", getPortTCP())
            );

         return false;
      }

      // check if adding a new server is allowed (in case this is a server)

      if(!RegisterNodeMsgEx::checkNewServerAllowed(servers, getNodeNumID(), nodeType) )
      { // this is a new server and adding was disabled
         LOG(GENERAL, WARNING, "Registration of new servers disabled, rejecting.", nodeID,
               nodeType);
         return true;
      }

      // construct node

      auto node = RegisterNodeMsgEx::constructNode(nodeType, nodeID, getNodeNumID(), getPortUDP(),
            getPortTCP(), nicList);

      std::string typedNodeID = node->getTypedNodeID();

      // add node to store (or update it)

      NumNodeID confirmationNodeNumID;

      NodeStoreResult nodeStoreRes = servers->addOrUpdateNodeEx(std::move(node), &confirmationNodeNumID);
      isNodeNew = (nodeStoreRes == NodeStoreResult::Added);
      if (nodeStoreRes == NodeStoreResult::Updated)
         heartbeatMgr->notifyAsyncRefreshNode(nodeID, getNodeNumID(), nodeType);

      if(confirmationNodeNumID != getNodeNumID() )
      { // unable to add node to store
         LOG(GENERAL, WARNING, "Node rejected because of ID conflict.", getNodeNumID(), getNodeID(),
               nodeType);
         return true;
      }

      // add to capacity pools

      if(nodeType == NODETYPE_Meta)
      {
         app->getMetaStateStore()->addIfNotExists(getNodeNumID().val(), CombinedTargetState(
            TargetReachabilityState_POFFLINE, TargetConsistencyState_GOOD) );

         bool isNewMetaTarget = metaCapacityPools->addIfNotExists(
            confirmationNodeNumID.val(), CapacityPool_LOW);

         if(isNewMetaTarget)
            heartbeatMgr->notifyAsyncAddedNode(nodeID, getNodeNumID(), nodeType);

         // (note: storage targets get published through MapTargetMsg)
      }

      // handle root node information (if any is given)
      RegisterNodeMsgEx::processIncomingRoot(getRootNumID(), nodeType, getRootIsBuddyMirrored());

   } // end of server heartbeat specific handling


   if(isNodeNew)
   { // this node is new
      RegisterNodeMsgEx::processNewNode(nodeID, getNodeNumID(), nodeType, &nicList, ctx.peerName());
   }

   // send response
   acknowledge(ctx);

   if (nodeType == NODETYPE_Meta)
      app->getMetaStateStore()->saveStatesToFile();
   else if (nodeType == NODETYPE_Storage)
      app->getTargetStateStore()->saveStatesToFile();

   return true;
}


