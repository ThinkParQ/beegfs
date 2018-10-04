#include "QuotaDataRequestor.h"
#include <common/storage/quota/Quota.h>
#include <program/Program.h>


/**
 * collects the quota data from the storage servers
 *
 * @param outQuotaData the map for the quota data separated for every target
 * @param outQuotaResultsRWLock the lock for outQuotaData
 *
 * @return true if the quota data successful collected from the storage servers
 */
bool QuotaDataRequestor::requestQuota(QuotaDataMapForTarget* outQuotaData,
   RWLock* outQuotaResultsRWLock)
{
   App* app = Program::getApp();
   NodeStoreServers* storageNodes = app->getStorageNodes();
   TargetMapper* mapper = app->getTargetMapper();

   if(this->cfg.cfgUseAll)
   {
      if(this->cfg.cfgType == QuotaDataType_USER)
         System::getAllUserIDs(&this->cfg.cfgIDList, !this->cfg.cfgWithSystemUsersGroups);
      else
         System::getAllGroupIDs(&this->cfg.cfgIDList, !this->cfg.cfgWithSystemUsersGroups);
   }

   //remove all duplicated IDs, the list::unique() needs a sorted list
   if (!this->cfg.cfgIDList.empty())
   {
      this->cfg.cfgIDList.sort();
      this->cfg.cfgIDList.unique();
   }

   QuotaDataMapForTarget tmpQuotaData;
   QuotaInodeSupport quotaInodeSupport;

   // ignore return value, update of quota data is required also when a target is off-line
   requestQuotaDataAndCollectResponses(storageNodes, &workQueue, &tmpQuotaData, mapper,
         &quotaInodeSupport);

   RWLockGuard const lock(*outQuotaResultsRWLock, SafeRWLock_WRITE);

   updateQuotaDataWithResponse(&tmpQuotaData, outQuotaData, mapper);

   return true;
}

/**
 * updates the quota data store with the new quota data and removes unknown targets from the quota
 * store
 *
 * @param inQuotaData the map with the new quota data map
 * @param outQuotaData the map with the old quota data map, which needs to be updated
 * @param mapper the target mapper with all known targets
 */
void QuotaDataRequestor::updateQuotaDataWithResponse(QuotaDataMapForTarget* inQuotaData,
   QuotaDataMapForTarget* outQuotaData, const TargetMapper* mapper)
{
   // replace existing quota data of a target or add new quota data of a target
   for(QuotaDataMapForTargetIter iter = inQuotaData->begin(); iter != inQuotaData->end(); iter++)
   {
      std::pair<QuotaDataMapForTargetIter, bool> retMapVal = outQuotaData->insert(*iter);
      if(!retMapVal.second)
      {
         outQuotaData->erase(retMapVal.first);
         outQuotaData->insert(*iter);
      }
   }

   // remove quota data of unknown targets, if the quota data are request for each target
   if(this->cfg.cfgTargetSelection == GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST_PER_TARGET)
   {
      UInt16List targetNumIDs;
      NumNodeIDList nodeNumIDs;
      mapper->getMappingAsLists(targetNumIDs, nodeNumIDs);

      for(QuotaDataMapForTargetIter iter = outQuotaData->begin(); iter != outQuotaData->end();)
      {
         bool targetExists = false;

         for(UInt16ListIter targetIter = targetNumIDs.begin(); targetIter != targetNumIDs.end();
            targetIter++)
         {
            if(iter->first == *targetIter)
            {
               targetExists = true;
               break;
            }
         }

         if(!targetExists)
         {
            QuotaDataMapForTargetIter tmpIter = iter;
            iter++;
            outQuotaData->erase(tmpIter);
         }
         else
            iter++;
      }
   }
}
