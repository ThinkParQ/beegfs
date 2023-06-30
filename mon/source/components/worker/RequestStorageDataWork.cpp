#include "RequestStorageDataWork.h"

#include <common/toolkit/MessagingTk.h>
#include <common/net/message/nodes/HeartbeatRequestMsg.h>
#include <common/net/message/mon/RequestStorageDataMsg.h>
#include <common/net/message/mon/RequestStorageDataRespMsg.h>
#include <components/StatsCollector.h>

void RequestStorageDataWork::process(char* bufIn, unsigned bufInLen,
      char* bufOut, unsigned bufOutLen)
{

   if (!node->getIsResponding())
   {
      HeartbeatRequestMsg heartbeatRequestMsg;

      if(MessagingTk::requestResponse(*node, heartbeatRequestMsg,
            NETMSGTYPE_Heartbeat))
      {
         LOG(GENERAL, DEBUG, "Node is responding again.",
               ("NodeID", node->getNodeIDWithTypeStr()));
         node->setIsResponding(true);
      }
   }

   Result result = {};
   result.data.isResponding = false;

   if (node->getIsResponding())
   {
      // generate the RequestStorageDataMsg with the lastStatsTime
      RequestStorageDataMsg requestDataMsg(node->getLastStatRequestTime().count());
      auto respMsg = MessagingTk::requestResponse(*node, requestDataMsg,
            NETMSGTYPE_RequestStorageDataResp);

      if (!respMsg)
      {
         LOG(GENERAL, DEBUG, "Node is not responding.", ("NodeID", node->getNodeIDWithTypeStr()));
         node->setIsResponding(false);
      }
      else
      {
         // get response and process it
         auto storageRspMsg = static_cast<RequestStorageDataRespMsg*>(respMsg.get());
         result.highResStatsList = std::move(storageRspMsg->getStatsList());
         result.storageTargetList = std::move(storageRspMsg->getStorageTargets());

         result.data.isResponding = true;
         result.data.indirectWorkListSize = storageRspMsg->getIndirectWorkListSize();
         result.data.directWorkListSize = storageRspMsg->getDirectWorkListSize();
         result.data.diskSpaceTotal = storageRspMsg->getDiskSpaceTotalMiB();
         result.data.diskSpaceFree = storageRspMsg->getDiskSpaceFreeMiB();
         result.data.sessionCount = storageRspMsg->getSessionCount();
         result.data.hostnameid = storageRspMsg->gethostnameid();

         if (!result.highResStatsList.empty())
         {
            auto lastStatsRequestTime = std::chrono::milliseconds(
                  result.highResStatsList.front().rawVals.statsTimeMS);
            node->setLastStatRequestTime(lastStatsRequestTime);
         }

         if (collectClientOpsByNode)
            result.ipOpsUnorderedMap = ClientOpsRequestor::request(*node, false);

         if (collectClientOpsByUser)
            result.userOpsUnorderedMap = ClientOpsRequestor::request(*node, true);
      }
   }

   result.node = std::move(node);

   statsCollector->insertStorageData(std::move(result));
}
