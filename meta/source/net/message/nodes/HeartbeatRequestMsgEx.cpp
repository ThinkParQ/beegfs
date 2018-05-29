#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include "HeartbeatRequestMsgEx.h"

bool HeartbeatRequestMsgEx::processIncoming(ResponseContext& ctx)
{
   //const char* logContext = "HeartbeatRequest incoming";

   //std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername(); 
   //LOG_DEBUG_CONTEXT(log, 5, std::string("Received a HeartbeatRequestMsg from: ") + peer);
   //IGNORE_UNUSED_VARIABLE(logContext);
   
   App* app = Program::getApp();
   Config* cfg = app->getConfig();
   
   Node& localNode = app->getLocalNode();
   NumNodeID localNodeNumID = localNode.getNumID();
   NumNodeID rootNodeID = app->getMetaNodes()->getRootNodeNumID();
   NicAddressList nicList(localNode.getNicList());
   const BitStore* nodeFeatureFlags = localNode.getNodeFeatures();

   HeartbeatMsg hbMsg(localNode.getID(), localNodeNumID, NODETYPE_Meta, &nicList,
      nodeFeatureFlags);
   hbMsg.setRootNumID(rootNodeID);
   hbMsg.setPorts(cfg->getConnMetaPortUDP(), cfg->getConnMetaPortTCP() );
   hbMsg.setFhgfsVersion(BEEGFS_VERSION_CODE);

   ctx.sendResponse(hbMsg);

   return true;
}

