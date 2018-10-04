#include "RequestStorageDataWork.h"
#include <program/Program.h>



void RequestStorageDataWork::process(char* bufIn, unsigned bufInLen,
   char* bufOut, unsigned bufOutLen)
{
   App *app = Program::getApp();

   // let's see at which time we did the last update to the HighResStats and
   // save the time
   HighResStatsList highResStats = node->getHighResData();
   uint64_t lastStatsTime = 0;

   if(!highResStats.empty())
   {
      lastStatsTime = highResStats.back().rawVals.statsTimeMS;
   }

   RequestStorageDataMsg requestDataMsg(lastStatsTime);
   const auto rspMsg = MessagingTk::requestResponse(*node, requestDataMsg,
      NETMSGTYPE_RequestStorageDataResp);

   // if node does not respond set not-responding flag, otherwise process the
   // response
   if (!rspMsg)
   {
      log.log(Log_SPAM, std::string("Received no response from node: ") +
         node->getNodeIDWithTypeStr());
      node->setNotResponding();
   }
   else
   {
      // get response and process it
      auto *storageAdmonDataMsg = (RequestStorageDataRespMsg*) rspMsg.get();
      std::string nodeID = storageAdmonDataMsg->getNodeID();
      NumNodeID nodeNumID = storageAdmonDataMsg->getNodeNumID();

      // create a new StorageNodeEx object and fill it with the parsed content
      // from the response 0,0 are dummy ports; the object will be deleted
      // anyways
      auto newNode = std::make_shared<StorageNodeEx>(nodeID, nodeNumID, 0, 0,
         storageAdmonDataMsg->getNicList());

      NodeStoreStorageEx *storageNodeStore = app->getStorageNodes();
      StorageNodeDataContent content = newNode->getContent();
      content.indirectWorkListSize = storageAdmonDataMsg->getIndirectWorkListSize();
      content.directWorkListSize = storageAdmonDataMsg->getDirectWorkListSize();
      content.diskSpaceTotal = storageAdmonDataMsg->getDiskSpaceTotalMiB();
      content.diskSpaceFree = storageAdmonDataMsg->getDiskSpaceFreeMiB();
      storageAdmonDataMsg->getStorageTargets().swap(content.storageTargets);
      newNode->setContent(content);

      // same with highResStats
      newNode->setHighResStatsList(storageAdmonDataMsg->getStatsList() );

      // call addOrUpdate in the node store (the newNode object will definitely
      // be deleted in there)
      storageNodeStore->addOrUpdateNode(std::move(newNode));
   }
}

