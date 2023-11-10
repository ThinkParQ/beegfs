#include "StatsCollector.h"

#include <common/toolkit/SocketTk.h>
#include <common/nodes/OpCounterTypes.h>

#include <app/App.h>


StatsCollector::StatsCollector(App* app) :
   PThread("StatsCollector"), app(app)
{}

void StatsCollector::run()
{
   try
   {
      LOG(GENERAL, DEBUG, "Component started.");
      registerSignalHandler();
      requestLoop();
      LOG(GENERAL, DEBUG, "Component stopped.");
   }
   catch (std::exception& e)
   {
      app->handleComponentException(e);
   }
}

void StatsCollector::requestLoop()
{
   bool collectClientOpsByNode = app->getConfig()->getCollectClientOpsByNode();
   bool collectClientOpsByUser = app->getConfig()->getCollectClientOpsByUser();

   // intially wait one query interval before requesting stats to give NodeListRequestor the time
   // to retrieve the node lists
   while (!waitForSelfTerminateOrder(std::chrono::milliseconds(
         app->getConfig()->getStatsRequestInterval()).count()))
   {
      {
         LOG(GENERAL, DEBUG, "Requesting Stats...");

         std::unique_lock<std::mutex> lock(mutex);

         workItemCounter = 0;
         metaResults.clear();
         storageResults.clear();

         // collect data

         const auto& metaNodes = app->getMetaNodes()->referenceAllNodes();

         for (auto node = metaNodes.begin(); node != metaNodes.end(); node++)
         {
            workItemCounter++;
            app->getWorkQueue()->addIndirectWork(
                  new RequestMetaDataWork(std::static_pointer_cast<MetaNodeEx>(*node),
                  this, collectClientOpsByNode, collectClientOpsByUser));
         }

         const auto& storageNodes = app->getStorageNodes()->referenceAllNodes();

         for (auto node = storageNodes.begin(); node != storageNodes.end(); node++)
         {
            workItemCounter++;
            app->getWorkQueue()->addIndirectWork(
                  new RequestStorageDataWork(std::static_pointer_cast<StorageNodeEx>(*node),
                  this, collectClientOpsByNode, collectClientOpsByUser));
         }

         while (workItemCounter > 0)
            condVar.wait(lock);

         // write data

         for (auto iter = metaResults.begin(); iter != metaResults.end(); iter++)
         {
            app->getTSDB()->insertMetaNodeData(iter->node, iter->data);

            for (auto listIter = iter->highResStatsList.begin();
                  listIter != iter->highResStatsList.end(); listIter++)
            {
               app->getTSDB()->insertHighResMetaNodeData(iter->node, *listIter);
            }

            if (collectClientOpsByNode)
            {
               for (auto mapIter = iter->ipOpsUnorderedMap.begin();
                     mapIter != iter->ipOpsUnorderedMap.end(); mapIter++)
               {
                  ipMetaClientOps.addOpsList(mapIter->first, mapIter->second);
               }
            }

            if (collectClientOpsByUser)
            {
               for (auto mapIter = iter->userOpsUnorderedMap.begin();
                     mapIter != iter->userOpsUnorderedMap.end(); mapIter++)
               {
                  userMetaClientOps.addOpsList(mapIter->first, mapIter->second);
               }
            }
         }

         for (auto iter = storageResults.begin(); iter != storageResults.end(); iter++)
         {
            app->getTSDB()->insertStorageNodeData(iter->node, iter->data);

            for (auto listIter = iter->highResStatsList.begin();
                  listIter != iter->highResStatsList.end(); listIter++)
            {
               app->getTSDB()->insertHighResStorageNodeData(iter->node, *listIter);
            }

            for (auto listIter = iter->storageTargetList.begin();
                  listIter != iter->storageTargetList.end();
                  listIter++)
            {
               app->getTSDB()->insertStorageTargetsData(iter->node, *listIter);
            }

            if (collectClientOpsByNode)
            {
               for (auto mapIter = iter->ipOpsUnorderedMap.begin();
                     mapIter != iter->ipOpsUnorderedMap.end(); mapIter++)
               {
                  ipStorageClientOps.addOpsList(mapIter->first, mapIter->second);
               }
            }

            if (collectClientOpsByUser)
            {
               for (auto mapIter = iter->userOpsUnorderedMap.begin();
                     mapIter != iter->userOpsUnorderedMap.end(); mapIter++)
               {
                  userStorageClientOps.addOpsList(mapIter->first, mapIter->second);
               }
            }
         }

         if (collectClientOpsByNode)
         {
            processClientOps(ipMetaClientOps, NODETYPE_Meta, false);
            processClientOps(ipStorageClientOps, NODETYPE_Storage, false);
         }

         if (collectClientOpsByUser)
         {
            processClientOps(userMetaClientOps, NODETYPE_Meta, true);
            processClientOps(userStorageClientOps, NODETYPE_Storage, true);
         }

         app->getTSDB()->write();
      }
   }
}

void StatsCollector::processClientOps(ClientOps& clientOps, NodeType nodeType, bool perUser)
{
   ClientOps::IdOpsMap diffOpsMap;
   ClientOps::OpsList sumOpsList;

   diffOpsMap = clientOps.getDiffOpsMap();
   sumOpsList = clientOps.getDiffSumOpsList();

   if (!diffOpsMap.empty())
   {
      for (auto opsMapIter = diffOpsMap.begin();
            opsMapIter != diffOpsMap.end();
            opsMapIter++)
      {
         std::string id;

         if (perUser)
         {
            if (opsMapIter->first == ~0U)
               id = "undefined";
            else
               id = StringTk::uintToStr(opsMapIter->first);
         }
         else
         {
            struct in_addr inAddr = { (in_addr_t)opsMapIter->first };
            id = Socket::ipaddrToStr(inAddr);
         }

         std::map<std::string, uint64_t> stringOpMap;
         unsigned opCounter = 0;
         for (auto opsListIter = opsMapIter->second.begin();
               opsListIter != opsMapIter->second.end();
               opsListIter++)
         {
            std::string opName;
            if (nodeType == NODETYPE_Meta)
               opName = OpToStringMapping::mapMetaOpNum(opCounter);
            else if (nodeType == NODETYPE_Storage)
               opName = OpToStringMapping::mapStorageOpNum(opCounter);

            stringOpMap[opName] = *opsListIter;
            opCounter++;
         }

         app->getTSDB()->insertClientNodeData(id, nodeType, stringOpMap, perUser);
      }
   }

   clientOps.clear();
}
