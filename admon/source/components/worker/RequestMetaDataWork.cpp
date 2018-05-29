#include <program/Program.h>
#include <common/toolkit/StringTk.h>
#include "RequestMetaDataWork.h"

void RequestMetaDataWork::process(char* bufIn, unsigned bufInLen,
   char* bufOut, unsigned bufOutLen)
{
   // let's see at which time we did the last update to the HighResStats and
   // save the time
   HighResStatsList highResStats = node->getHighResData();
   uint64_t lastStatsTime = 0;
   if (!highResStats.empty())
   {
      lastStatsTime = highResStats.back().rawVals.statsTimeMS;
   }

   // generate the RequestDataMsg with the lastStatsTime
   RequestMetaDataMsg requestDataMsg(lastStatsTime);
   char *outBuf;
   NetMessage *rspMsg = NULL;
   bool respReceived = MessagingTk::requestResponse(*node, &requestDataMsg,
      NETMSGTYPE_RequestMetaDataResp, &outBuf, &rspMsg);

   // if node does not respond set not-responding flag, otherwise process the
   // response
   if (!respReceived)
   {
      log.log(Log_SPAM, std::string("Received no response from node: ") +
         node->getNodeIDWithTypeStr() );
      node->setNotResponding();
   }
   else
   {
      // get response and process it
      RequestMetaDataRespMsg *metaAdmonDataMsg =
         (RequestMetaDataRespMsg*) rspMsg;
      std::string nodeID = metaAdmonDataMsg->getNodeID();
      NumNodeID nodeNumID = metaAdmonDataMsg->getNodeNumID();
      App *app = Program::getApp();

      bool isRoot = metaAdmonDataMsg->getIsRoot();

      // create a new MetaNodeEx object and fill it with the parsed content from the response
      // 0,0 are dummy ports; the object will be deleted anyways
      auto newNode = std::make_shared<MetaNodeEx>(nodeID, nodeNumID, 0, 0,
            metaAdmonDataMsg->getNicList());
      MetaNodeDataContent content = newNode->getContent();
      content.isRoot = isRoot;
      content.indirectWorkListSize = metaAdmonDataMsg->getIndirectWorkListSize();
      content.directWorkListSize = metaAdmonDataMsg->getDirectWorkListSize();
      content.sessionCount = metaAdmonDataMsg->getSessionCount();
      newNode->setContent(content);

      // same with highResStats
      newNode->setHighResStatsList(metaAdmonDataMsg->getStatsList() );

      // call addOrUpdate in the node store (the newNode object will definitely
      // be deleted in there)
      NodeStoreMetaEx *metaNodeStore = app->getMetaNodes();
      metaNodeStore->addOrUpdateNode(std::move(newNode));

      SAFE_FREE(outBuf);
      SAFE_DELETE(metaAdmonDataMsg);
   }
}
