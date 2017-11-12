#include <common/net/msghelpers/MsgHelperAck.h>
#include <common/net/sock/NetworkInterfaceCard.h>
#include <program/Program.h>
#include "HeartbeatMsgEx.h"

bool HeartbeatMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   LogContext log("Heartbeat incoming");

   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   //LOG_DEBUG_CONTEXT(log, Log_DEBUG, std::string("Received a HeartbeatMsg from: ") + peer);

   App* app = Program::getApp();
   bool isNodeNew;

   // construct node

   NicAddressList nicList;
   parseNicList(&nicList);
   Node* node = new Node(getNodeID(), getNodeNumID(), getPortUDP(), getPortTCP(),
      nicList); // (will belong to the NodeStore => no delete() required)

   node->setNodeType(getNodeType() );
   node->setFhgfsVersion(getFhgfsVersion() );

   // set local nic capabilities

   NicAddressList localNicList(app->getLocalNicList() );
   NicListCapabilities localNicCaps;

   NetworkInterfaceCard::supportedCapabilities(&localNicList, &localNicCaps);
   node->getConnPool()->setLocalNicCaps(&localNicCaps);
   
   std::string nodeIDWithTypeStr = node->getNodeIDWithTypeStr();


   // add/update node in store

   AbstractNodeStore* nodes = app->getAbstractNodeStoreFromType(getNodeType() );
   if(!nodes)
   {
      log.logErr("Invalid node type: " + StringTk::intToStr(getNodeType() ) +
         "(" + Node::nodeTypeToStr(getNodeType() ) + ")");
      
      goto ack_resp;
   }

   isNodeNew = nodes->addOrUpdateNode(&node);
   if( (isNodeNew) && (getNodeType() != NODETYPE_Client) )
   { // log info about new server
      bool supportsSDP = NetworkInterfaceCard::supportsSDP(&nicList);
      bool supportsRDMA = NetworkInterfaceCard::supportsRDMA(&nicList);

      log.log(Log_WARNING, std::string("New node: ") +
         nodeIDWithTypeStr + "; " +
         std::string(supportsSDP ? "SDP; " : "") +
         std::string(supportsRDMA ? "RDMA; " : "") );

      log.log(Log_DEBUG, "Number of nodes: "
         "Meta: " + StringTk::intToStr(app->getMetaNodes()->getSize() ) + "; "
         "Storage: " + StringTk::intToStr(app->getStorageNodes()->getSize() ) );
   }


   processIncomingRoot();

ack_resp:
   MsgHelperAck::respondToAckRequest(this, fromAddr, sock,
      respBuf, bufLen, app->getDatagramListener() );

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
   if(Program::getApp()->getMetaNodes()->setRootNodeNumID(getRootNumID(), false) )
   {
      log.log(Log_CRITICAL, "Root (by Heartbeat): " + StringTk::uintToStr(getRootNumID() ) );
      
      Program::getApp()->getRootDir()->setOwnerNodeID(getRootNumID() );
   }
}

