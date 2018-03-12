#include <common/net/sock/NetworkInterfaceCard.h>
#include <program/Program.h>
#include "HeartbeatMsgEx.h"

bool HeartbeatMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("Heartbeat incoming");

   //LOG_DEBUG_CONTEXT(log, 4, std::string("Received a HeartbeatMsg from: ") + peer);

   App* app = Program::getApp();
   
   bool isNodeNew;

   // construct node

   NicAddressList& nicList = getNicList();

   auto node = std::make_shared<Node>(getNodeID(), getNodeNumID(), getPortUDP(), getPortTCP(),
      nicList);

   node->setNodeType(getNodeType() );
   node->setFhgfsVersion(getFhgfsVersion() );

   node->setFeatureFlags(&getNodeFeatureFlags() );

   // set local nic capabilities

   Node& localNode = app->getLocalNode();
   NicAddressList localNicList(localNode.getNicList() );
   NicListCapabilities localNicCaps;

   NetworkInterfaceCard::supportedCapabilities(&localNicList, &localNicCaps);
   node->getConnPool()->setLocalNicCaps(&localNicCaps);

   std::string nodeIDWithTypeStr = node->getNodeIDWithTypeStr();

   // get corresponding node store
   
   AbstractNodeStore* nodes = app->getAbstractNodeStoreFromType(getNodeType() );
   if(unlikely(!nodes) )
   {
      LOG(ERR, "Invalid node type.",
            as("Node Type", Node::nodeTypeToStr(getNodeType())),
            as("Sender", ctx.peerName()),
            as("NodeID", getNodeID()),
            as("Port (UDP)", getPortUDP()),
            as("Port (TCP)", getPortTCP())
         );
      
      goto ack_resp;
   }

   // add/update node in store

   isNodeNew = nodes->addOrUpdateNode(std::move(node));
   if(isNodeNew)
      processNewNode(nodeIDWithTypeStr, getNodeType(), &nicList, ctx.peerName() );


   processIncomingRoot();

ack_resp:
   acknowledge(ctx);

   return true;
}

/**
 * Handles the contained root information.
 */
void HeartbeatMsgEx::processIncomingRoot()
{
   // check whether root info is defined
   if( (getNodeType() != NODETYPE_Meta) || !getRootNumID() )
      return;

   // try to apply the contained root info
   Program::getApp()->getMetaNodes()->setRootNodeNumID(getRootNumID(), false,
      getRootIsBuddyMirrored());
}

/**
 * @param nodeIDWithTypeStr for log msg (the string that is returned by
 * node->getNodeIDWithTypeStr() ).
 */
void HeartbeatMsgEx::processNewNode(std::string nodeIDWithTypeStr, NodeType nodeType,
   NicAddressList* nicList, std::string sourcePeer)
{
   LogContext log("Node registration");

   App* app = Program::getApp();
   AbstractNodeStore* nodes = app->getAbstractNodeStoreFromType(getNodeType() );

   bool supportsSDP = NetworkInterfaceCard::supportsSDP(nicList);
   bool supportsRDMA = NetworkInterfaceCard::supportsRDMA(nicList);
   std::string nodeTypeStr = Node::nodeTypeToStr(nodeType);

   log.log(Log_WARNING, std::string("New node: ") +
      nodeIDWithTypeStr + "; " +
      std::string(supportsSDP ? "SDP; " : "") +
      std::string(supportsRDMA ? "RDMA; " : "") +
      std::string("Source: ") + sourcePeer);

   log.log(Log_DEBUG, std::string("Number of nodes in the system: ") +
      StringTk::uintToStr(nodes->getSize() ) +
      std::string(" (Type: ") + nodeTypeStr + ")" );
}
