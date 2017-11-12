#include "RequestMetaDataMsgEx.h"
#include <program/Program.h>

bool RequestMetaDataMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats *stats)
{
   LogContext log("RequestMetaDataMsg incoming");

   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG_CONTEXT(log, 4, std::string("Received a RequestDataMsg from: ") + peer);

   App *app = Program::getApp();
   Node *node = app->getLocalNode();
   NodeStoreEx *metaNodeStore = app->getMetaNodes();
   MultiWorkQueue *workQueue = app->getWorkQueue();

   unsigned sessionCount = app->getSessions()->getSize();
   
   NicAddressList nicList(node->getNicList());

   // highresStats
   HighResStatsList statsHistory;
   uint64_t lastStatsMS = getValue();

   // get stats history
   StatsCollector* statsCollector = app->getStatsCollector();
   statsCollector->getStatsSince(lastStatsMS, statsHistory);

   std::string nodeID = node->getID();

   RequestMetaDataRespMsg requestMetaDataRespMsg(nodeID.c_str(), node->getNumID(), &nicList,
      metaNodeStore->gotRoot(), workQueue->getIndirectWorkListSize(),
      workQueue->getDirectWorkListSize(), sessionCount, &statsHistory);
   requestMetaDataRespMsg.serialize(respBuf, bufLen);
   
   sock->sendto(respBuf, requestMetaDataRespMsg.getMsgLength(), 0, (struct sockaddr*) fromAddr,
      sizeof(struct sockaddr_in));
   
   LOG_DEBUG_CONTEXT(log, 5, std::string("Sent a message with type: " ) +
      StringTk::uintToStr(requestMetaDataRespMsg.getMsgType() ) + std::string(" to admon") );
   
   app->getClientOpStats()->updateNodeOp(sock->getPeerIP(), MetaOpCounter_REQUESTMETADATA);

   return true;
}
