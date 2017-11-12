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

   NicAddressList localNicList(app->getLocalNicList() );
   NicListCapabilities localNicCaps;

   NetworkInterfaceCard::supportedCapabilities(&localNicList, &localNicCaps);
   node->getConnPool()->setLocalNicCaps(&localNicCaps);
   
   std::string nodeIDWithTypeStr = node->getNodeIDWithTypeStr();


   // add/update node in store

   AbstractNodeStore* nodes;
   
   switch(getNodeType() )
   {
      case NODETYPE_Meta:
         nodes = app->getMetaNodes(); break;
      
      case NODETYPE_Mgmt:
         nodes = app->getMgmtNodes(); break;

      case NODETYPE_Storage:
         nodes = app->getStorageNodes(); break;

      default:
      {
         log.logErr("Invalid/unexpected node type: " + Node::nodeTypeToStr(getNodeType() ) );
         
         goto ack_resp;
      } break;
   }

   isNodeNew = nodes->addOrUpdateNode(std::move(node));
   if(isNodeNew)
   { // log info about new server
      bool supportsSDP = NetworkInterfaceCard::supportsSDP(&nicList);
      bool supportsRDMA = NetworkInterfaceCard::supportsRDMA(&nicList);

      log.log(Log_WARNING, std::string("New node: ") +
         nodeIDWithTypeStr + "; " +
         std::string(supportsSDP ? "SDP; " : "") +
         std::string(supportsRDMA ? "RDMA; " : "") );
   }


ack_resp:
   acknowledge(ctx);

   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), StorageOpCounter_HEARTBEAT,
      getMsgHeaderUserID() );

   return true;
}

