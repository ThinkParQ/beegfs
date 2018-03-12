#include <common/components/worker/GetQuotaInfoWork.h>
#include <common/net/message/storage/quota/GetQuotaInfoRespMsg.h>
#include <common/toolkit/SynchronizedCounter.h>
#include "GetQuotaInfo.h"


typedef std::map<uint16_t, std::shared_ptr<Mutex>> MutexMap;


/*
 * send the quota data requests to the storage nodes and collects the responses
 *
 * @param mgmtNode the management node, need to be referenced
 * @param storageNodes a NodeStore with all storage servers
 * @param workQ the MultiWorkQueue to use
 * @param outQuotaResults returns the quota informations
 * @param mapper the target mapper (only required for GETQUOTACONFIG_SINGLE_TARGET and
 *        GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST_PER_TARGET)
 * @param requestLimits if true, the quota limits will be downloaded from the management server and
 *        not the used quota will be downloaded from the storage servers
 * @return error information, true on success, false if a error occurred
 *
 */
bool GetQuotaInfo::requestQuotaDataAndCollectResponses(const NodeHandle& mgmtNode,
   NodeStoreServers* storageNodes, MultiWorkQueue* workQ, QuotaDataMapForTarget* outQuotaResults,
   TargetMapper* mapper, bool requestLimits, QuotaInodeSupport* quotaInodeSupport)
{
   bool retVal = true;

   int numWorks = 0;
   SynchronizedCounter counter;

   int maxMessageCount = getMaxMessageCount();

   UInt16Vector nodeResults;
   std::vector<NodeHandle> allNodes;

   MutexMap mutexMap;

   if(requestLimits)
   { /* download the limits from the management daemon, multiple messages will be used if the
        payload of the message is to small */

      nodeResults = UInt16Vector(maxMessageCount);

      QuotaDataMap map;
      outQuotaResults->insert(QuotaDataMapForTargetMapVal(QUOTADATAMAPFORTARGET_ALL_TARGETS_ID,
         map) );

      //send command to the storage servers and print the response
      // request all subranges (=> msg size limitation) from current server
      mutexMap.insert({ QUOTADATAMAPFORTARGET_ALL_TARGETS_ID, std::make_shared<Mutex>() });

      for(int messageNumber = 0; messageNumber < maxMessageCount; messageNumber++)
      {
         Work* work = new GetQuotaInfoWork(this->cfg, mgmtNode, messageNumber,
            &outQuotaResults->find(QUOTADATAMAPFORTARGET_ALL_TARGETS_ID)->second,
            mutexMap[QUOTADATAMAPFORTARGET_ALL_TARGETS_ID].get(), &counter,
            &nodeResults[numWorks], quotaInodeSupport);
         workQ->addDirectWork(work);

         numWorks++;
      }

      *quotaInodeSupport = QuotaInodeSupport_UNKNOWN;
   }
   else
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
               &nodeResults[numWorks], quotaInodeSupport);
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
                  &nodeResults[numWorks], quotaInodeSupport);
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
               quotaInodeSupport);
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
   if(!requestLimits && (this->cfg.cfgTargetSelection == GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST) )
   {
      QuotaDataMap map;

      for(QuotaDataMapForTargetIter storageIter = outQuotaResults->begin();
         storageIter != outQuotaResults->end(); storageIter++ )
      {
         //merge the QuotaData from the responses with the existing quotaData
         for(QuotaDataMapIter resultIter = storageIter->second.begin();
            resultIter != storageIter->second.end(); resultIter++)
         {
            QuotaDataMapIter outIter = map.find(resultIter->first);
            if(outIter != map.end())
               outIter->second.mergeQuotaDataCounter(&(resultIter->second) );
            else
               map.insert(QuotaDataMapVal(resultIter->first, resultIter->second) );
         }
      }

      outQuotaResults->clear();
      outQuotaResults->insert(QuotaDataMapForTargetMapVal(QUOTADATAMAPFORTARGET_ALL_TARGETS_ID,
         map) );
   }

   for (UInt16VectorIter iter = nodeResults.begin(); iter != nodeResults.end(); iter++)
   {
      if (*iter != 0)
      {
         // delete QuotaDataMap with invalid QuotaData
         if(this->cfg.cfgTargetSelection != GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST)
            outQuotaResults->erase(*iter);

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

