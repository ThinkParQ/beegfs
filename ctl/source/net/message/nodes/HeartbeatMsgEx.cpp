#include <common/net/sock/NetworkInterfaceCard.h>
#include <program/Program.h>
#include "HeartbeatMsgEx.h"

#include <boost/lexical_cast.hpp>

bool HeartbeatMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();

   bool isNodeNew;

   // construct node

   NicAddressList& nicList = getNicList();

   auto node = std::make_shared<Node>(getNodeType(), getNodeID(), getNodeNumID(), getPortUDP(),
         getPortTCP(), nicList);

   // set local nic capabilities

   Node& localNode = app->getLocalNode();
   NicAddressList localNicList(localNode.getNicList() );
   NicListCapabilities localNicCaps;

   NetworkInterfaceCard::supportedCapabilities(&localNicList, &localNicCaps);
   node->getConnPool()->setLocalNicList(localNicList, localNicCaps);

   std::string nodeIDWithTypeStr = node->getNodeIDWithTypeStr();

   // get corresponding node store

   AbstractNodeStore* nodes = app->getAbstractNodeStoreFromType(getNodeType() );
   if(unlikely(!nodes) )
   {
      LOG(GENERAL, ERR, "Invalid node type.",
            ("Node Type", getNodeType()),
            ("Sender", ctx.peerName()),
            ("NodeID", getNodeID()),
            ("Port (UDP)", getPortUDP()),
            ("Port (TCP)", getPortTCP())
         );

      goto ack_resp;
   }

   // add/update node in store

   isNodeNew = (nodes->addOrUpdateNode(std::move(node)) == NodeStoreResult::Added);
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
   Program::getApp()->getMetaRoot().setIfDefault(getRootNumID(), getRootIsBuddyMirrored());
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
   std::string nodeTypeStr = boost::lexical_cast<std::string>(nodeType);

   log.log(Log_WARNING, std::string("New node: ") +
      nodeIDWithTypeStr + "; " +
      std::string(supportsSDP ? "SDP; " : "") +
      std::string(supportsRDMA ? "RDMA; " : "") +
      std::string("Source: ") + sourcePeer);

   log.log(Log_DEBUG, std::string("Number of nodes in the system: ") +
      StringTk::uintToStr(nodes->getSize() ) +
      std::string(" (Type: ") + nodeTypeStr + ")" );
}
