#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "HeartbeatRequestMsgEx.h"

bool HeartbeatRequestMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();
   DatagramListener* dgramLis = app->getDatagramListener();

   Node& localNode = app->getLocalNode();
   NicAddressList nicList(localNode.getNicList() );


   HeartbeatMsg hbMsg(localNode.getID(), NumNodeID(0), NODETYPE_Client, &nicList);
   hbMsg.setPorts(dgramLis->getUDPPort(), 0);
   ctx.sendResponse(hbMsg);

   return true;
}

