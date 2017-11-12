#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "HeartbeatRequestMsgEx.h"

bool HeartbeatRequestMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   //const char* logContext = "HeartbeatRequest incoming";

   //std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername(); 
   //LOG_DEBUG_CONTEXT(log, 5, std::string("Received a HeartbeatRequestMsg from: ") + peer);
   //IGNORE_UNUSED_VARIABLE(logContext);
   
   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   
   Node* localNode = app->getLocalNode();
   std::string localNodeID = localNode->getID();
   uint16_t localNodeNumID = localNode->getNumID();
   uint16_t rootNodeID = app->getMetaNodes()->getRootNodeNumID();
   NicAddressList nicList(localNode->getNicList() );
   
   HeartbeatMsg hbMsg(localNodeID.c_str(), localNodeNumID, NODETYPE_Meta, rootNodeID, &nicList);
   hbMsg.setPorts(cfg->getConnMetaPortUDP(), cfg->getConnMetaPortTCP() );
   hbMsg.setFhgfsVersion(FHGFS_VERSION_CODE);
   
   hbMsg.serialize(respBuf, bufLen);
      
   if(fromAddr)
   { // datagram => reply via dgramLis send method
      app->getDatagramListener()->sendto(respBuf, hbMsg.getMsgLength(), 0,
         (struct sockaddr*)fromAddr, sizeof(*fromAddr) );
   }
   else
      sock->sendto(respBuf, hbMsg.getMsgLength(), 0, NULL, 0);

   return true;
}

