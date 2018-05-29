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
   Config* cfg = app->getConfig();

   Node& localNode = app->getLocalNode();
   NumNodeID localNodeNumID = localNode.getNumID();
   NicAddressList nicList(localNode.getNicList() );
   const BitStore* nodeFeatureFlags = localNode.getNodeFeatures();

   HeartbeatMsg hbMsg(localNode.getID(), localNodeNumID, NODETYPE_Mgmt, &nicList,
      nodeFeatureFlags);
   hbMsg.setPorts(cfg->getConnMgmtdPortUDP(), cfg->getConnMgmtdPortTCP() );
   hbMsg.setFhgfsVersion(BEEGFS_VERSION_CODE);

   ctx.sendResponse(hbMsg);

   return true;
}

