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
   NumNodeID rootNodeID = app->getMetaRoot().getID();
   NicAddressList nicList(localNode.getNicList());

   HeartbeatMsg hbMsg(localNode.getAlias(), localNodeNumID, NODETYPE_Meta, &nicList);
   hbMsg.setRootNumID(rootNodeID);
   hbMsg.setPorts(cfg->getConnMetaPort(), cfg->getConnMetaPort() );

   ctx.sendResponse(hbMsg);

   return true;
}

