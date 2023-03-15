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
   Node& localNode = app->getLocalNode();
   NicAddressList localNicList(localNode.getNicList());
   NicListCapabilities localNicCaps;

   NetworkInterfaceCard::supportedCapabilities(&localNicList, &localNicCaps);
   node->getConnPool()->setLocalNicList(localNicList, localNicCaps);

   std::string nodeIDWithTypeStr = node->getNodeIDWithTypeStr();


   // add/update node in store
   NodeStore* nodes = NULL;

   switch(getNodeType() )
   {
      case NODETYPE_Meta:
         nodes = app->getMetaNodes(); break;

      case NODETYPE_Storage:
         nodes = app->getStorageNodes(); break;


      case NODETYPE_Mgmt:
         nodes = app->getMgmtNodes(); break;

      default:
      {
         LOG(GENERAL, ERR, "Invalid node type.",
               ("Node Type", getNodeType()),
               ("Sender", ctx.peerName()),
               ("NodeID", getNodeNumID()),
               ("Port (UDP)", getPortUDP()),
               ("Port (TCP)", getPortTCP())
            );

         goto ack_resp;
      } break;
   }


   isNodeNew = (nodes->addOrUpdateNode(std::move(node)) == NodeStoreResult::Added);
   if(isNodeNew)
   {
      bool supportsSDP = NetworkInterfaceCard::supportsSDP(&nicList);
      bool supportsRDMA = NetworkInterfaceCard::supportsRDMA(&nicList);

      log.log(Log_WARNING, std::string("New node: ") +
         nodeIDWithTypeStr + "; " +
         std::string(supportsSDP ? "SDP; " : "") +
         std::string(supportsRDMA ? "RDMA; " : "") +
         std::string("Source: ") + ctx.peerName() );

      log.log(Log_DEBUG, std::string("Number of nodes in the system: ") +
         StringTk::intToStr(nodes->getSize() ) +
         std::string(" (Type: ") + boost::lexical_cast<std::string>(getNodeType()) + ")");
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
   }
}
