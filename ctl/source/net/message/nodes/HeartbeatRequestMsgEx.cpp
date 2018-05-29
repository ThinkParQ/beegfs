#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "HeartbeatRequestMsgEx.h"

bool HeartbeatRequestMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("HeartbeatRequest incoming");

   //std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   //LOG_DEBUG_CONTEXT(log, 5, std::string("Received a HeartbeatRequestMsg from: ") + peer);

   App* app = Program::getApp();
   DatagramListener* dgramLis = app->getDatagramListener();

   Node& localNode = app->getLocalNode();
   NicAddressList nicList(localNode.getNicList() );
   const BitStore* nodeFeatureFlags = localNode.getNodeFeatures();


   HeartbeatMsg hbMsg(localNode.getID(), NumNodeID(0), NODETYPE_Client, &nicList,
      nodeFeatureFlags);
   hbMsg.setPorts(dgramLis->getUDPPort(), 0);
   ctx.sendResponse(hbMsg);

   return true;
}

