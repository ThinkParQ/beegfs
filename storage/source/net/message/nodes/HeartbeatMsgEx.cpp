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
         log.logErr("Invalid/unexpected node type: "
               + boost::lexical_cast<std::string>(getNodeType()));

         goto ack_resp;
      } break;
   }

   isNodeNew = (nodes->addOrUpdateNode(std::move(node)) == NodeStoreResult::Added);
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

