#include "RequestStorageDataMsgEx.h"

bool RequestStorageDataMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();
   Node& node = app->getLocalNode();
   MultiWorkQueueMap* workQueueMap = app->getWorkQueueMap();
   StorageTargets* storageTargets = app->getStorageTargets();

   // get disk space of each target

   StorageTargetInfoList storageTargetInfoList;

   storageTargets->generateTargetInfoList(storageTargetInfoList);

   // compute total disk space and total free space

   int64_t diskSpaceTotal = 0; // sum of all targets
   int64_t diskSpaceFree = 0; // sum of all targets

   for(StorageTargetInfoListIter iter = storageTargetInfoList.begin();
       iter != storageTargetInfoList.end();
       iter++)
   {
      if(diskSpaceTotal == -1)
         continue; // statfs() failed on this target

      diskSpaceTotal += iter->getDiskSpaceTotal();
      diskSpaceFree += iter->getDiskSpaceFree();
   }


   unsigned sessionCount = app->getSessions()->getSize();

   NicAddressList nicList(node.getNicList());
   std::string hostnameid = System::getHostname();

   // highresStats
   HighResStatsList statsHistory;
   uint64_t lastStatsMS = getValue();

   // get stats history
   StatsCollector* statsCollector = app->getStatsCollector();
   statsCollector->getStatsSince(lastStatsMS, statsHistory);

   // get work queue stats
   unsigned indirectWorkListSize = 0;
   unsigned directWorkListSize = 0;

   for(MultiWorkQueueMapCIter iter = workQueueMap->begin(); iter != workQueueMap->end(); iter++)
   {
      indirectWorkListSize += iter->second->getIndirectWorkListSize();
      directWorkListSize += iter->second->getDirectWorkListSize();
   }

   RequestStorageDataRespMsg requestStorageDataRespMsg(node.getID(), hostnameid, node.getNumID(),
      &nicList, indirectWorkListSize, directWorkListSize, diskSpaceTotal, diskSpaceFree,
      sessionCount, &statsHistory, &storageTargetInfoList);
   ctx.sendResponse(requestStorageDataRespMsg);

   LOG_DEBUG(__func__, Log_SPAM, std::string("Sent a message with type: " ) +
      StringTk::uintToStr(requestStorageDataRespMsg.getMsgType() ) + std::string(" to mon") );

   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(),
      StorageOpCounter_REQUESTSTORAGEDATA, getMsgHeaderUserID() );

   return true;
}
