#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "HeartbeatRequestMsgEx.h"

bool HeartbeatRequestMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("Heartbeat request incoming");
   App* app = Program::getApp();
   Config* cfg = app->getConfig();

   Node& localNode = app->getLocalNode();
   NumNodeID localNodeNumID = localNode.getNumID();
   NicAddressList nicList(localNode.getNicList() );

   HeartbeatMsg hbMsg(localNode.getID(), localNodeNumID, NODETYPE_Storage, &nicList);
   hbMsg.setPorts(cfg->getConnStoragePortUDP(), cfg->getConnStoragePortTCP() );

   ctx.sendResponse(hbMsg);

   log.log(Log_DEBUG, std::string("Heartbeat req ip:") + StringTk::uintToHexStr(ctx.getSocket()->getPeerIP()));

   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), StorageOpCounter_HEARTBEAT,
      getMsgHeaderUserID() );

   return true;
}

