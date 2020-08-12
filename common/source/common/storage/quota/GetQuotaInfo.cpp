#include <common/components/worker/GetQuotaInfoWork.h>
#include <common/net/message/storage/quota/GetQuotaInfoRespMsg.h>
#include <common/nodes/StoragePoolStore.h>
#include <common/toolkit/SynchronizedCounter.h>
#include "GetQuotaInfo.h"


typedef std::map<uint16_t, std::shared_ptr<Mutex>> MutexMap;


/*
 * send the quota limit requests to the management node and collects the responses
 *
 * note: the usage of storage pools here will only work if cfgTargetSelection  is set to
 * GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST_PER_TARGET
 *
 * @param mgmtNode the management node, need to be referenced
 * @param workQ the MultiWorkQueue to use
 * @param outQuotaResults returns the quota informations
 * @param mapper the target mapper (only required for GETQUOTACONFIG_SINGLE_TARGET and
 *        GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST_PER_TARGET)
 * @param storagePoolId the storagePool to get information for; is only relevant if
 *        storagePoolStore is !NULL
 * @return error information, true on success, false if a error occurred
 */
bool GetQuotaInfo::requestQuotaLimitsAndCollectResponses(const NodeHandle& mgmtNode,
   MultiWorkQueue* workQ, QuotaDataMapForTarget* outQuotaResults, const TargetMapper* mapper,
   StoragePoolId storagePoolId)
{
   bool retVal = true;

   SynchronizedCounter counter;

   int maxMessageCount = getMaxMessageCount();

   UInt16Vector nodeResults(maxMessageCount);
   QuotaInodeSupport quotaInodeSupport;

   Mutex mutex;

   auto& resultMap = (*outQuotaResults)[QUOTADATAMAPFORTARGET_ALL_TARGETS_ID];

   for (int messageNumber = 0; messageNumber < maxMessageCount; messageNumber++)
   {
      Work* work = new GetQuotaInfoWork(cfg, mgmtNode, messageNumber, &resultMap, &mutex,
            &counter, &nodeResults[messageNumber], &quotaInodeSupport, storagePoolId);
      workQ->addDirectWork(work);
   }

   // wait for all work to be done
   counter.waitForCount(maxMessageCount);

   for (auto iter = nodeResults.begin(); iter != nodeResults.end(); iter++)
   {
      if (*iter != 0)
      {
         // delete QuotaDataMap with invalid QuotaData
         if (cfg.cfgTargetSelection != GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST)
            outQuotaResults->erase(*iter);
         retVal = false;
      }
   }

   return retVal;
}

/*
 * send the quota data requests to the storage nodes and collects the responses
 *
 * note: the usage of storage pools here will only work if cfgTargetSelection  is set to
 * GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST_PER_TARGET
 *
 * @param storageNodes a NodeStore with all storage servers
 * @param workQ the MultiWorkQueue to use
 * @param outQuotaResults returns the quota informations
 * @param mapper the target mapper (only required for GETQUOTACONFIG_SINGLE_TARGET and
 *        GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST_PER_TARGET)
 * @param storagePoolStore if !NULL the variable storagePoolId will be considered and only targets
 *        belonging to the given storage pool will be accounted
 * @param storagePoolId the storagePool to get information for; is only relevant if
 *        storagePoolStore is !NULL
 * @return error information, true on success, false if a error occurred
 */
bool GetQuotaInfo::requestQuotaDataAndCollectResponses(const NodeStoreServers* storageNodes,
   MultiWorkQueue* workQ, QuotaDataMapForTarget* outQuotaResults, const TargetMapper* mapper,
   QuotaInodeSupport* quotaInodeSupport, StoragePoolStore* storagePoolStore,
   StoragePoolId storagePoolId)
{
   bool retVal = true;

   int numWorks = 0;
   SynchronizedCounter counter;

   int maxMessageCount = getMaxMessageCount();

   UInt16Vector nodeResults;
   std::vector<NodeHandle> allNodes;

   MutexMap mutexMap;

   if(this->cfg.cfgTargetSelection == GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST)
   { /* download the used quota from the storage daemon, one request for all targets, multiple
        messages will be used if the payload of the message is to small */

      allNodes = storageNodes->referenceAllNodes();

      nodeResults = UInt16Vector(maxMessageCount * storageNodes->getSize() );

      // request all subranges (=> msg size limitation) from current server
      for(int messageNumber = 0; messageNumber < maxMessageCount; messageNumber++)
      {
         for (auto nodeIt = allNodes.begin(); nodeIt != allNodes.end(); ++nodeIt)
         {
            //send command to the storage servers and print the response
            auto& storageNode = *nodeIt;

            if(messageNumber == 0)
            {
               QuotaDataMap map;
               outQuotaResults->insert(QuotaDataMapForTargetMapVal(
                  storageNode->getNumID().val(), map) );
            }

            mutexMap.insert({ storageNode->getNumID().val(), std::make_shared<Mutex>() });

            Work* work = new GetQuotaInfoWork(this->cfg, storageNode, messageNumber,
               &outQuotaResults->find(storageNode->getNumID().val())->second,
               mutexMap[storageNode->getNumID().val()].get(), &counter,
               &nodeResults[numWorks], quotaInodeSupport, storagePoolId);
            workQ->addDirectWork(work);

            numWorks++;
         }
      }
   }
   else
   if(this->cfg.cfgTargetSelection == GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST_PER_TARGET)
   { /* download the used quota from the storage daemon, a single request for each target, multiple
        messages will be used if the payload of the message is to small */

      allNodes = storageNodes->referenceAllNodes();

      nodeResults = UInt16Vector(maxMessageCount * mapper->getSize() );

      // if storage pool store is set, we need to reference the pool to check later, if received
      // targets are members of the pool
      // note: if pool ID doesn't exist, the whole routine works, as if no pool ID was given
      StoragePoolPtr storagePool;
      if (storagePoolStore)
        storagePool = storagePoolStore->getPool(storagePoolId);

      // request all subranges (=> msg size limitation) from current server
      for(int messageNumber = 0; messageNumber < maxMessageCount; messageNumber++)
      {
         for (auto nodeIt = allNodes.begin(); nodeIt != allNodes.end(); ++nodeIt)
         {
            auto& storageNode = *nodeIt;

            //send command to the storage servers
            UInt16List targetIDs;
            mapper->getTargetsByNode(storageNode->getNumID(), targetIDs);

            // request the quota data for the targets in a separate message
            for(UInt16ListIter iter = targetIDs.begin(); iter != targetIDs.end(); iter++)
            {
               // if a storage pool was set and this target doesn't belong to the requested pool,
               // then skip it
               if (storagePool && !storagePool->hasTarget(*iter))
                  continue;

               GetQuotaInfoConfig requestCfg = this->cfg;
               requestCfg.cfgTargetNumID = *iter;

               if(messageNumber == 0)
               {
                  QuotaDataMap map;
                  outQuotaResults->insert(QuotaDataMapForTargetMapVal(*iter, map) );
               }

               mutexMap.insert({ *iter, std::make_shared<Mutex>() });

               Work* work = new GetQuotaInfoWork(requestCfg, storageNode, messageNumber,
                  &outQuotaResults->at(*iter), mutexMap[*iter].get(), &counter,
                  &nodeResults[numWorks], quotaInodeSupport, storagePoolId);
               workQ->addDirectWork(work);

               numWorks++;
            }
         }
      }
   }
   else
   if(this->cfg.cfgTargetSelection == GETQUOTACONFIG_SINGLE_TARGET)
   { /* download the used quota from the storage daemon, request the data only for a single target,
        multiple messages will be used if the payload of the message is to small */
      NumNodeID storageNumID = mapper->getNodeID(this->cfg.cfgTargetNumID);

      allNodes.push_back(storageNodes->referenceNode(storageNumID));
      auto& storageNode = allNodes.front();

      // use IntVector because it doesn't work with BoolVector. std::vector<bool> is not a container
      nodeResults = UInt16Vector(maxMessageCount);

      QuotaDataMap map;
      outQuotaResults->insert(QuotaDataMapForTargetMapVal(this->cfg.cfgTargetNumID, map) );

      //send command to the storage servers
      if(storageNode != NULL)
      {
         mutexMap.insert({ cfg.cfgTargetNumID, std::make_shared<Mutex>() });

         // request all subranges (=> msg size limitation) from current server
         for(int messageNumber = 0; messageNumber < maxMessageCount; messageNumber++)
         {
            Work* work = new GetQuotaInfoWork(this->cfg, storageNode, messageNumber,
               &outQuotaResults->find(this->cfg.cfgTargetNumID)->second,
               mutexMap[cfg.cfgTargetNumID].get(), &counter, &nodeResults[numWorks],
               quotaInodeSupport, storagePoolId);
            workQ->addDirectWork(work);

            numWorks++;
         }
      }
      else
      {
         LogContext("GetQuotaInfo").logErr("Unknown target selection.");
         nodeResults[numWorks] = 0;
      }
   }

   // wait for all work to be done
   counter.waitForCount(numWorks);

   // merge the quota data if required
   if (cfg.cfgTargetSelection == GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST)
   {
      QuotaDataMap map = calculateQuotaSums(*outQuotaResults);

      outQuotaResults->clear();
      outQuotaResults->insert(QuotaDataMapForTargetMapVal(QUOTADATAMAPFORTARGET_ALL_TARGETS_ID,
         map) );
   }

   for (UInt16VectorIter iter = nodeResults.begin(); iter != nodeResults.end(); iter++)
   {
      if (*iter != 0)
      {
         // delete QuotaDataMap with invalid QuotaData
         if(this->cfg.cfgTargetSelection != GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST) {
            outQuotaResults->erase(*iter);
            LOG(QUOTA, ERR, "Unable to fetch quota information from storage target."
                  " Is the node offline?", ("Storage target id", *iter));
         } else {
            LOG(QUOTA, ERR, "Unable to fetch quota information from storage server."
                  " Is the node offline?", ("Storage node id", *iter));
         }

         retVal = false;
      }
   }

   return retVal;
}

/*
 * calculate the number of messages which are required to download all quota data
 */
int GetQuotaInfo::getMaxMessageCount()
{

   unsigned numIds = 1;

   if(this->cfg.cfgUseList || this->cfg.cfgUseAll)
   {
      numIds = cfg.cfgIDList.size();
   }
   else if (cfg.cfgUseRange)
   {
      numIds = this->cfg.cfgIDRangeEnd - this->cfg.cfgIDRangeStart + 1; //inclusive range
   }

   int retVal = numIds / GETQUOTAINFORESPMSG_MAX_ID_COUNT;

  if (numIds % GETQUOTAINFORESPMSG_MAX_ID_COUNT != 0)
         ++retVal;

   return retVal;
}

QuotaDataMap GetQuotaInfo::calculateQuotaSums(const QuotaDataMapForTarget& quotaMaps)
{
   QuotaDataMap map;

   for (QuotaDataMapForTargetConstIter quotaMapsIter = quotaMaps.begin();
      quotaMapsIter != quotaMaps.end(); quotaMapsIter++ )
   {
      for(QuotaDataMapConstIter resultIter = quotaMapsIter->second.begin();
         resultIter != quotaMapsIter->second.end(); resultIter++)
      {
         QuotaDataMapIter outIter = map.find(resultIter->first);
         if(outIter != map.end())
            outIter->second.mergeQuotaDataCounter(&(resultIter->second) );
         else
            map.insert(QuotaDataMapVal(resultIter->first, resultIter->second) );
      }
   }

   return map;
}
