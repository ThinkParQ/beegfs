#include "RequestMetaDataWork.h"

#include <common/toolkit/MessagingTk.h>
#include <common/net/message/nodes/HeartbeatRequestMsg.h>
#include <common/net/message/admon/RequestMetaDataMsg.h>
#include <common/net/message/admon/RequestMetaDataRespMsg.h>
#include <components/StatsCollector.h>

void RequestMetaDataWork::process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{

   if (!node->getIsResponding())
   {
      HeartbeatRequestMsg heartbeatRequestMsg;
      char* respBuf;
      NetMessage* respMsg;
      if(MessagingTk::requestResponse(*node, &heartbeatRequestMsg,
            NETMSGTYPE_Heartbeat, &respBuf, &respMsg))
      {
         LOG(GENERAL, DEBUG, "Node is responding again.",
               as("NodeID", node->getNodeIDWithTypeStr()));
         node->setIsResponding(true);
      }
      SAFE_FREE(respBuf);
      SAFE_DELETE(respMsg);
   }

   Result result = {};
   result.data.isResponding = false;

   if (node->getIsResponding())
   {
      // generate the RequestDataMsg with the lastStatsTime
      RequestMetaDataMsg requestDataMsg(node->getLastStatRequestTime().count());
      char* respBuf;
      NetMessage* respMsg;
      bool commRes;
      commRes = MessagingTk::requestResponse(*node, &requestDataMsg,
            NETMSGTYPE_RequestMetaDataResp, &respBuf, &respMsg);

      // make sure respBuf and respMsg are gonna be deleted
      std::unique_ptr<char, decltype(free)*> respBufPtr(respBuf, free);
      std::unique_ptr<NetMessage> respMsgPtr(respMsg);


      if (!commRes)
      {
         LOG(GENERAL, DEBUG, "Node is not responding.", as("NodeID", node->getNodeIDWithTypeStr()));
         node->setIsResponding(false);
      }
      else
      {
         // get response and process it
         auto metaRspMsg = static_cast<RequestMetaDataRespMsg*>(respMsg);
         result.highResStatsList = std::move(metaRspMsg->getStatsList());

         result.data.isResponding = true;
         result.data.indirectWorkListSize = metaRspMsg->getIndirectWorkListSize();
         result.data.directWorkListSize = metaRspMsg->getDirectWorkListSize();
         result.data.sessionCount = metaRspMsg->getSessionCount();

         if (!result.highResStatsList.empty())
         {
            auto lastStatsRequestTime = std::chrono::milliseconds(
                  result.highResStatsList.front().rawVals.statsTimeMS);
            node->setLastStatRequestTime(lastStatsRequestTime);
         }

         result.idOpsUnorderedMap = ClientOpsRequestor::request(*node, false);
      }
   }

   result.node = std::move(node);

   statsCollector->insertMetaData(std::move(result));
}