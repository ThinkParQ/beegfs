#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "HeartbeatRequestMsgEx.h"

bool HeartbeatRequestMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();
   Config* cfg = app->getConfig();

   Node& localNode = app->getLocalNode();
   NumNodeID localNodeNumID = localNode.getNumID();
   NicAddressList nicList(localNode.getNicList() );

   LOG(GENERAL, DEBUG, std::string("Heartbeat request incoming: ") + StringTk::uintToHexStr(ctx.getSocket()->getPeerIP()));

   HeartbeatMsg hbMsg(localNode.getID(), localNodeNumID, NODETYPE_Mgmt, &nicList);
   hbMsg.setPorts(cfg->getConnMgmtdPortUDP(), cfg->getConnMgmtdPortTCP() );

   ctx.sendResponse(hbMsg);

   return true;
}

