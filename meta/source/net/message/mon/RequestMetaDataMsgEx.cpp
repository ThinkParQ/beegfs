#include <common/components/worker/queue/MultiWorkQueue.h>
#include <program/Program.h>
#include <session/SessionStore.h>
#include "RequestMetaDataMsgEx.h"

bool RequestMetaDataMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("RequestMetaDataMsg incoming");

   App *app = Program::getApp();
   Node& node = app->getLocalNode();
   MultiWorkQueue *workQueue = app->getWorkQueue();

   unsigned sessionCount = app->getSessions()->getSize() + app->getMirroredSessions()->getSize();

   NicAddressList nicList(node.getNicList());
   std::string hostnameid = System::getHostname();

   // highresStats
   HighResStatsList statsHistory;
   uint64_t lastStatsMS = getValue();

   // get stats history
   StatsCollector* statsCollector = app->getStatsCollector();
   statsCollector->getStatsSince(lastStatsMS, statsHistory);

   RequestMetaDataRespMsg requestMetaDataRespMsg(node.getID(), hostnameid, node.getNumID(), &nicList,
      app->getMetaRoot().getID() == node.getNumID(), workQueue->getIndirectWorkListSize(),
      workQueue->getDirectWorkListSize(), sessionCount, &statsHistory);
   ctx.sendResponse(requestMetaDataRespMsg);

   LOG_DEBUG_CONTEXT(log, 5, std::string("Sent a message with type: " ) +
      StringTk::uintToStr(requestMetaDataRespMsg.getMsgType() ) + std::string(" to mon") );

   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), MetaOpCounter_REQUESTMETADATA,
      getMsgHeaderUserID() );

   return true;
}
