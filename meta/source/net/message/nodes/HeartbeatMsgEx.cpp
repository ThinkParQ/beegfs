#include <common/net/sock/NetworkInterfaceCard.h>
#include <program/Program.h>
#include "HeartbeatMsgEx.h"

#include <boost/lexical_cast.hpp>

bool HeartbeatMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("Heartbeat incoming");

   App* app = Program::getApp();
   bool isNodeNew;

   // construct node

   NicAddressList& nicList = getNicList();

   auto node = std::make_shared<Node>(getNodeType(), getNodeID(), getNodeNumID(), getPortUDP(),
         getPortTCP(), nicList);

   // set local nic capabilities

   NicAddressList localNicList(app->getLocalNicList() );
   NicListCapabilities localNicCaps;

   NetworkInterfaceCard::supportedCapabilities(&localNicList, &localNicCaps);
   node->getConnPool()->setLocalNicList(localNicList, localNicCaps);

   std::string nodeIDWithTypeStr = node->getNodeIDWithTypeStr();

   log.log(Log_DEBUG, std::string("Heartbeat node: ") + nodeIDWithTypeStr);

   // add/update node in store
   AbstractNodeStore* nodes = app->getAbstractNodeStoreFromType(getNodeType() );
   if(!nodes)
   {
      log.logErr("Invalid node type: " + StringTk::intToStr(getNodeType() ) +
         "(" + boost::lexical_cast<std::string>(getNodeType()) + ")");

      goto ack_resp;
   }

   isNodeNew = (nodes->addOrUpdateNode(std::move(node)) == NodeStoreResult::Added);
   if( (isNodeNew) && (getNodeType() != NODETYPE_Client) )
   { // log info about new server
      bool supportsSDP = NetworkInterfaceCard::supportsSDP(&nicList);
      bool supportsRDMA = NetworkInterfaceCard::supportsRDMA(&nicList);

      std::string supports;
      if (supportsSDP && !supportsRDMA)
         supports = "; SDP.";
      else if (supportsSDP && supportsRDMA)
         supports = "; SDP; RDMA.";
      else if (supportsRDMA)
         supports = "; RDMA.";


      log.log(Log_WARNING, std::string("New node: ") + nodeIDWithTypeStr + supports);

      log.log(Log_DEBUG, "Number of nodes: "
         "Meta: " + StringTk::intToStr(app->getMetaNodes()->getSize() ) + "; "
         "Storage: " + StringTk::intToStr(app->getStorageNodes()->getSize() ) );
   }

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
   LogContext log("Heartbeat incoming");

   // check whether root info is defined
   if( (getNodeType() != NODETYPE_Meta) || !getRootNumID() )
      return;

   // try to apply the contained root info
   if(Program::getApp()->getMetaRoot().setIfDefault(getRootNumID(), getRootIsBuddyMirrored()))
   {
      log.log(Log_CRITICAL, "Root (by Heartbeat): " + getRootNumID().str() );

      Program::getApp()->getRootDir()->setOwnerNodeID(getRootNumID() );
      if (getRootIsBuddyMirrored())
         Program::getApp()->getRootDir()->setIsBuddyMirroredFlag(true);

   }
}

