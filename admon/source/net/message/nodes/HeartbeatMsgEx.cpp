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
   NicAddressList localNicList(localNode.getNicList() );
   NicListCapabilities localNicCaps;

   NetworkInterfaceCard::supportedCapabilities(&localNicList, &localNicCaps);
   node->getConnPool()->setLocalNicCaps(&localNicCaps);

   std::string nodeIDWithTypeStr = node->getNodeIDWithTypeStr();


   // add/update node in store

   NodeStoreServers* nodes = nullptr;

   switch(getNodeType() )
   {
      case NODETYPE_Meta:
         nodes = app->getMetaNodes(); break;

      case NODETYPE_Storage:
         nodes = app->getStorageNodes(); break;

      case NODETYPE_Mgmt:
         nodes = app->getMgmtNodes(); break;

      // Might come from ctl.
      case NODETYPE_Client:
         break;

      default:
      {
         LOG(GENERAL, ERR, "Invalid node type.",
               ("Node Type", getNodeType()),
               ("Sender", ctx.peerName()),
               ("NodeID", getNodeNumID()),
               ("Port (UDP)", getPortUDP()),
               ("Port (TCP)", getPortTCP())
            );

      } break;
   }

   if(nodes != nullptr)
   {
      isNodeNew = nodes->addOrUpdateNodeEx(std::move(node), NULL);

      if(isNodeNew)
      { // log info about new server
         bool supportsSDP = NetworkInterfaceCard::supportsSDP(&nicList);
         bool supportsRDMA = NetworkInterfaceCard::supportsRDMA(&nicList);

         log.log(Log_WARNING, std::string("New node: ") +
            nodeIDWithTypeStr + "; " +
            std::string(supportsSDP ? "SDP; " : "") +
            std::string(supportsRDMA ? "RDMA; " : "") +
            std::string("Source: ") + ctx.peerName() );

         log.log(Log_WARNING,
            "Number of nodes: " + StringTk::intToStr(nodes->getSize() ) + " "
            "(Type: " + boost::lexical_cast<std::string>(getNodeType()) + ")");
      }

      processIncomingRoot();
   }

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

   const bool rootIsBuddyMirrored = getRootIsBuddyMirrored();

   // try to apply the contained root info
   Program::getApp()->getMetaRoot().setIfDefault(getRootNumID(), rootIsBuddyMirrored);
}

