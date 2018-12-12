#include <app/App.h>
#include <common/components/worker/DummyWork.h>
#include <common/threading/PThread.h>
#include <common/threading/RWLockGuard.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <common/storage/quota/QuotaData.h>
#include <components/worker/SetExceededQuotaWork.h>
#include <program/Program.h>
#include <storage/StoragePoolEx.h>
#include "QuotaManager.h"


#include <fstream>
#include <iostream>
#include <sys/file.h>

const std::string QuotaManager::QUOTA_SAVE_DIR            = "quota";
const std::string QuotaManager::USER_LIMITS_FILENAME      = "quotaUserLimits.store";
const std::string QuotaManager::GROUP_LIMITS_FILENAME     = "quotaGroupLimits.store";
const std::string QuotaManager::DEFAULT_LIMITS_FILENAME   = "quotaDefaultLimits.store";

// relative paths inside mgmt directory
const std::string QuotaManager::USED_QUOTA_USER_PATH  = QUOTA_SAVE_DIR +
                                                        "/" + "quotaDataUser.store";
const std::string QuotaManager::USED_QUOTA_GROUP_PATH = QUOTA_SAVE_DIR +
                                                        "/" + "quotaDataGroup.store";

/**
 * Constructor.
 */
QuotaManager::QuotaManager()
 : PThread("QuotaMgr"),
   log("QuotaManager")
{
   storagePoolStore = Program::getApp()->getStoragePoolStore();

   // load the quota data and the quota limits if possible
   loadQuotaData();

   const unsigned numWorkers = Program::getApp()->getConfig()->getTuneNumQuotaWorkers();

   for (unsigned i = 0; i < numWorkers; i++)
   {
      std::unique_ptr<Worker> worker = std::unique_ptr<Worker>(new Worker(
         std::string("QuotaWorker") + StringTk::uintToStr(i+1), &workQueue, QueueWorkType_DIRECT));

      workerList.push_back(std::move(worker));
   }
}

/**
 * Destructor.
 */
QuotaManager::~QuotaManager()
{
   workerList.clear();

}

/**
 * Run method of the thread.
 */
void QuotaManager::run()
{
   try
   {
      registerSignalHandler();

      for (auto iter = workerList.begin(); iter != workerList.end(); iter++)
         (*iter)->start();

      requestLoop();

      for (auto iter = workerList.begin(); iter != workerList.end(); iter++)
      {
         (*iter)->selfTerminate();

         // add dummy work to wake up the worker immediately for faster self termination
         PersonalWorkQueue* personalQueue = (*iter)->getPersonalWorkQueue();
         workQueue.addPersonalWork(new DummyWork(), personalQueue);
      }

      for (auto iter = workerList.begin(); iter != workerList.end(); iter++)
         (*iter)->join();

      log.log(Log_DEBUG, "Component stopped.");
   }
   catch(std::exception& e)
   {
      Program::getApp()->handleComponentException(e);
   }
}

/**
 * The implementation of the thread loop. It checks the used quota, calculate the exceeded quota IDs
 * and push the IDs to the servers.
 */
void QuotaManager::requestLoop()
{
   Config* cfg = Program::getApp()->getConfig();

   const int sleepIntervalMS = 5*1000; // 5sec

   const unsigned updateQuotaUsageMS = cfg->getQuotaUpdateIntervalMin()*60*1000; // min to millisec
   const unsigned saveQuotaDataMS = cfg->getQuotaStoreIntervalMin()*60*1000; // min to millisec

   bool firstUpdate = true;

   Time lastQuotaUsageUpdateT;
   Time lastQuotaDataSaveT;

   while(!waitForSelfTerminateOrder(sleepIntervalMS) )
   {
      if(firstUpdate || (lastQuotaUsageUpdateT.elapsedMS() > updateQuotaUsageMS) )
      {
         bool doUpdate = true;
         UIntList uidList;
         UIntList gidList;

         if(cfg->getQuotaQueryTypeNum() == MgmtQuotaQueryType_FILE)
         {
            doUpdate = doUpdate && loadIdsFromFileWithReTry(cfg->getQuotaQueryUIDFile(),
               READ_ID_FILE_NUM_RETRIES, uidList);
            doUpdate = doUpdate && loadIdsFromFileWithReTry(cfg->getQuotaQueryGIDFile(),
               READ_ID_FILE_NUM_RETRIES, gidList);
         }

         if(doUpdate)
         {
            updateUsedQuota(uidList, gidList);

            StoragePoolExPtrVec storagePools = storagePoolStore->getPoolsAsVec();
            for (auto iter = storagePools.begin(); iter != storagePools.end(); iter++)
            {
               calculateExceededQuota(**iter, uidList, gidList);
               (*iter)->quotaLimitChanged.compareAndSet(0, 1);  // reset flag for updated limits
            }

            pushExceededQuotaIDs();
            lastQuotaUsageUpdateT.setToNow();
         }
      }
      else
      {
         // this is not the first iteration and used quota doesn't have to be updated, because
         // the timeout is not reached yet
         // => for each pool check if a limit was updated, if yes calculate the the exceeded
         // quota IDs (afterwards push the IDs to the servers).
         // Do not update the used quota data for performance reason
         StoragePoolExPtrVec storagePools = storagePoolStore->getPoolsAsVec();
         for (auto iter = storagePools.begin(); iter != storagePools.end(); iter++)
         {
            bool limitsHaveChanged = (*iter)->quotaLimitChanged.compareAndSet(0, 1);

            if (limitsHaveChanged)
            { //
               bool doUpdate = true;
               UIntList uidList;
               UIntList gidList;

               if (cfg->getQuotaQueryTypeNum() == MgmtQuotaQueryType_FILE)
               {
                  doUpdate = doUpdate
                     && loadIdsFromFileWithReTry(cfg->getQuotaQueryUIDFile(),
                        READ_ID_FILE_NUM_RETRIES, uidList);
                  doUpdate = doUpdate
                     && loadIdsFromFileWithReTry(cfg->getQuotaQueryGIDFile(),
                        READ_ID_FILE_NUM_RETRIES, gidList);
               }

               if (doUpdate)
               {
                  calculateExceededQuota(**iter, uidList, gidList);
                  pushExceededQuotaIDs(**iter);
               }
            }
         }
      }

      if(firstUpdate || (lastQuotaDataSaveT.elapsedMS() > saveQuotaDataMS) )
      {
         saveQuotaData();
         lastQuotaDataSaveT.setToNow();
      }

      firstUpdate = false;
   }

   saveQuotaData();
}

/**
 * Removes a target from the quota data store, required for target unmap and remove node.
 *
 * @param targetNumID The targetNumID  of the target to remove.
 */
void QuotaManager::removeTargetFromQuotaDataStores(uint16_t targetNumID)
{
   {
      RWLockGuard const lock(usedQuotaUserRWLock, SafeRWLock_WRITE);

      usedQuotaUser.erase(targetNumID);
   }

   {
      RWLockGuard const lock(usedQuotaGroupRWLock, SafeRWLock_WRITE);

      usedQuotaGroup.erase(targetNumID);
   }
}

/**
 * Adds or updates the quota limits with the given list.
 *
 * @param storagePoolId the ID of the storage pool
 * @param list the list with the new quota limits.
 */
void QuotaManager::updateQuotaLimits(StoragePoolId storagePoolId, const QuotaDataList& list)
{
   // get the appropriate storage pool
   std::shared_ptr<StoragePoolEx> storagePool = storagePoolStore->getPool(storagePoolId);

   if (!storagePool)
   {
      LOG(QUOTA, WARNING, "Could not update quota limits, because storage pool doesn't exist.",
         storagePoolId);
      return;
   }

   if(list.begin()->getType() == QuotaDataType_USER)
   {
      storagePool->quotaUserLimits->addOrUpdateLimits(list);
      storagePool->quotaUserLimits->saveToFile();
   }
   else
   {
      storagePool->quotaGroupLimits->addOrUpdateLimits(list);
      storagePool->quotaGroupLimits->saveToFile();
   }

   // the calculation of the exceeded quota is done in the run loop
   // Note: don't do the calculation here because the beegfs-ctl command doesn't finish before
   // the calculation and the update of the server is done in this case
   storagePool->quotaLimitChanged.set(1);
}

/**
 * Updates the default user quota limits.
 *
 * @param storagePoolId the ID of the storage pool
 * @param inodeLimit The new default quota inode limit.
 * @param sizeLimit The new default quota size limit.
 * @return True if the default limits could be stored successful.
 */
bool QuotaManager::updateDefaultUserLimits(StoragePoolId storagePoolId, size_t inodeLimit,
      size_t sizeLimit)
{
   // get the appropriate storage pool
   StoragePoolExPtr storagePool = storagePoolStore->getPool(storagePoolId);

   if (!storagePool)
   {
      LOG(QUOTA, WARNING, "Could not update default user quota limits, "
                   "because storage pool doesn't exist.",
                   storagePoolId);
      return false;
   }

   storagePool->quotaDefaultLimits->updateUserLimits(inodeLimit, sizeLimit);
   bool saveRes = storagePool->quotaDefaultLimits->saveToFile();

   // the calculation of the exceeded quota is done in the run loop
   // Note: don't do the calculation here because the beegfs-ctl command doesn't finish before
   // the calculation and the update of the server is done in this case
   if (saveRes)
   {
      storagePool->quotaLimitChanged.set(1);
      return true;
   }
   else
   {
      return false;
   }
}

/**
 * Updates the default group quota limits.
 *
 * @param storagePoolId the ID of the storage pool
 * @param inodeLimit The new default quota inode limit.
 * @param sizeLimit The new default quota size limit.
 * @return True if the default limits could be stored successful.
 */
bool QuotaManager::updateDefaultGroupLimits(StoragePoolId storagePoolId, size_t inodeLimit,
      size_t sizeLimit)
{
   // get the appropriate storage pool
   StoragePoolExPtr storagePool = storagePoolStore->getPool(storagePoolId);

   if (!storagePool)
   {
      LOG(QUOTA, WARNING, "Could not update default group quota limits, "
                   "because storage pool doesn't exist.",
                   storagePoolId);
      return false;
   }

   storagePool->quotaDefaultLimits->updateGroupLimits(inodeLimit, sizeLimit);
   bool saveRes = storagePool->quotaDefaultLimits->saveToFile();

   // the calculation of the exceeded quota is done in the run loop
   // Note: don't do the calculation here because the beegfs-ctl command doesn't finish before
   // the calculation and the update of the server is done in this case
   if (saveRes)
   {
      storagePool->quotaLimitChanged.set(1);
      return true;
   }
   else
   {
      return false;
   }
}

/**
 * Requests the quota data from the storage servers and updates the quota data store.
 *
 * @param uidList A list with the UIDs to update, contains values if the IDs are provided by a file.
 * @param gidList A list with the GIDs to update, contains values if the IDs are provided by a file.
 */
bool QuotaManager::updateUsedQuota(UIntList& uidList, UIntList& gidList)
{
   bool retValUser = false;
   bool retValGroup = false;

   Config* cfg = Program::getApp()->getConfig();

   QuotaDataRequestor* requestorUser;
   QuotaDataRequestor* requestorGroup;

   if(cfg->getQuotaQueryTypeNum() == MgmtQuotaQueryType_SYSTEM)
   {
      requestorUser = new QuotaDataRequestor(workQueue, QuotaDataType_USER,
         cfg->getQuotaQueryWithSystemUsersGroups() );
      requestorGroup = new QuotaDataRequestor(workQueue, QuotaDataType_GROUP,
         cfg->getQuotaQueryWithSystemUsersGroups() );
   }
   else if(cfg->getQuotaQueryTypeNum() == MgmtQuotaQueryType_FILE)
   {
      requestorUser = new QuotaDataRequestor(workQueue, QuotaDataType_USER, uidList);
      requestorGroup = new QuotaDataRequestor(workQueue, QuotaDataType_GROUP, gidList);
   }
   else
   {
      requestorUser = new QuotaDataRequestor(workQueue, QuotaDataType_USER,
         cfg->getQuotaQueryUIDRangeStart(), cfg->getQuotaQueryUIDRangeEnd(),
         cfg->getQuotaQueryWithSystemUsersGroups() );
      requestorGroup = new QuotaDataRequestor(workQueue, QuotaDataType_GROUP,
         cfg->getQuotaQueryGIDRangeStart(), cfg->getQuotaQueryGIDRangeEnd(),
         cfg->getQuotaQueryWithSystemUsersGroups() );
   }


   retValUser = requestorUser->requestQuota(&usedQuotaUser, &this->usedQuotaUserRWLock);

   if (!retValUser)
      log.log(Log_ERR, "Could not update user quota data.");


   retValGroup = requestorGroup->requestQuota(&usedQuotaGroup, &this->usedQuotaGroupRWLock);

   if (!retValGroup)
      log.log(Log_ERR, "Could not update group quota data.");

   SAFE_DELETE(requestorUser);
   SAFE_DELETE(requestorGroup);

   return (retValUser && retValGroup);
}

/**
 * Calculates the IDs with exceeded quota and stores the IDs in the exceeded quota store for a
 * given pool.
 *
 * @param storagePool
 * @param uidList A list with the UIDs to check, contains values if the IDs are provided by a file.
 * @param gidList A list with the GIDs to check, contains values if the IDs are provided by a file.
 */
void QuotaManager::calculateExceededQuota(StoragePoolEx& storagePool, UIntList& uidList,
   UIntList& gidList)
{
   Config* cfg = Program::getApp()->getConfig();

   UIntList exceededUIDsSize;
   UIntList exceededUIDsInodes;

   UIntList systemUIDs;
   UIntList systemGIDs;

   const QuotaDefaultLimits& defaultLimits = *(storagePool.quotaDefaultLimits);

   bool defaultLimitsConfigured =
         (defaultLimits.getDefaultUserQuotaInodes() ||
          defaultLimits.getDefaultUserQuotaSize() ||
          defaultLimits.getDefaultGroupQuotaInodes() ||
          defaultLimits.getDefaultGroupQuotaSize());

   if (cfg->getQuotaQueryTypeNum() == MgmtQuotaQueryType_SYSTEM)
   {
      System::getAllUserIDs(&systemUIDs, !cfg->getQuotaQueryWithSystemUsersGroups());
      System::getAllGroupIDs(&systemGIDs, !cfg->getQuotaQueryWithSystemUsersGroups());
   }

   // calculate exceeded quota for users
   QuotaDataMap userLimits = storagePool.quotaUserLimits->getAllQuotaLimits();

   {
      RWLockGuard const lock(usedQuotaUserRWLock, SafeRWLock_READ);

      if(defaultLimitsConfigured)
      { // default quota is configured, requires to check every UID
         if( (cfg->getQuotaQueryTypeNum() == MgmtQuotaQueryType_SYSTEM) ||
            (cfg->getQuotaQueryTypeNum() == MgmtQuotaQueryType_FILE) )
         { // use the users of the system
            const auto& list = cfg->getQuotaQueryTypeNum() == MgmtQuotaQueryType_SYSTEM
               ? systemUIDs
               : uidList;

            for (auto iter = list.begin(); iter != list.end(); iter++)
            {
               if (isQuotaExceededForIDunlocked(storagePool, *iter, QuotaLimitType_SIZE,
                        usedQuotaUser, userLimits, defaultLimits.getDefaultUserQuotaSize()))
               {
                  exceededUIDsSize.push_back(*iter);
               }

               if (isQuotaExceededForIDunlocked(storagePool, *iter, QuotaLimitType_INODE,
                   usedQuotaUser, userLimits, defaultLimits.getDefaultUserQuotaInodes()))
               {
                  exceededUIDsInodes.push_back(*iter);
               }
            }
         }
         else
         { // use the users from a configured range
            for(unsigned index = cfg->getQuotaQueryUIDRangeStart();
               index <= cfg->getQuotaQueryUIDRangeEnd(); index++)
            {
               if (isQuotaExceededForIDunlocked(storagePool, index, QuotaLimitType_SIZE,
                        usedQuotaUser, userLimits, defaultLimits.getDefaultUserQuotaSize()))
               {
                  exceededUIDsSize.push_back(index);
               }

               if (isQuotaExceededForIDunlocked(storagePool, index, QuotaLimitType_INODE,
                   usedQuotaUser, userLimits, defaultLimits.getDefaultUserQuotaInodes()))
               {
                  exceededUIDsInodes.push_back(index);
               }
            }
         }
      }
      else
      { // no default quota configured, check only UIDs which has a configured limit
         for(QuotaDataMapIter iter = userLimits.begin(); iter != userLimits.end(); iter++)
         {
            if (isQuotaExceededForIDunlocked(storagePool, iter->first, QuotaLimitType_SIZE,
                usedQuotaUser, userLimits, defaultLimits.getDefaultUserQuotaSize()))
            {
               exceededUIDsSize.push_back(iter->first);
            }

            if (isQuotaExceededForIDunlocked(storagePool, iter->first, QuotaLimitType_INODE,
                usedQuotaUser, userLimits, defaultLimits.getDefaultUserQuotaInodes()))
            {
               exceededUIDsInodes.push_back(iter->first);
            }
         }
      }
   }

   storagePool.exceededQuotaStore.updateExceededQuota(&exceededUIDsSize, QuotaDataType_USER,
      QuotaLimitType_SIZE);
   storagePool.exceededQuotaStore.updateExceededQuota(&exceededUIDsInodes, QuotaDataType_USER,
      QuotaLimitType_INODE);


   UIntList exceededGIDsSize;
   UIntList exceededGIDsInodes;

   QuotaDataMap groupLimits = storagePool.quotaGroupLimits->getAllQuotaLimits();

   // calculate exceeded quota for groups
   {
      RWLockGuard const lock(usedQuotaGroupRWLock, SafeRWLock_READ);

      if(defaultLimitsConfigured)
      { // default quota is configured, requires to check every GID
         if( (cfg->getQuotaQueryTypeNum() == MgmtQuotaQueryType_SYSTEM) ||
            (cfg->getQuotaQueryTypeNum() == MgmtQuotaQueryType_FILE) )
         { // use the groups of the system
            const auto& list = cfg->getQuotaQueryTypeNum() == MgmtQuotaQueryType_SYSTEM
               ? systemGIDs
               : gidList;

            for (auto iter = list.begin(); iter != list.end(); iter++)
            {
               if (isQuotaExceededForIDunlocked(storagePool, *iter, QuotaLimitType_SIZE,
                  usedQuotaGroup, groupLimits, defaultLimits.getDefaultGroupQuotaSize()))
               {
                  exceededGIDsSize.push_back(*iter);
               }

               if (isQuotaExceededForIDunlocked(storagePool, *iter, QuotaLimitType_INODE,
                  usedQuotaGroup, groupLimits, defaultLimits.getDefaultGroupQuotaInodes()))
               {
                  exceededGIDsInodes.push_back(*iter);
               }
            }
         }
         else
         { // use the groups from a configured range
            for(unsigned index = cfg->getQuotaQueryGIDRangeStart();
               index <= cfg->getQuotaQueryGIDRangeEnd(); index++)
            {
               if (isQuotaExceededForIDunlocked(storagePool, index, QuotaLimitType_SIZE,
                  usedQuotaGroup, groupLimits, defaultLimits.getDefaultGroupQuotaSize()))
               {
                  exceededGIDsSize.push_back(index);
               }

               if (isQuotaExceededForIDunlocked(storagePool, index, QuotaLimitType_INODE,
                  usedQuotaGroup, groupLimits, defaultLimits.getDefaultGroupQuotaInodes()))
               {
                  exceededGIDsInodes.push_back(index);
               }
            }
         }
      }
      else
      { // no default quota configured, check only GIDs which has a configured limit
         for(QuotaDataMapIter iter = groupLimits.begin(); iter != groupLimits.end(); iter++)
         {
            if (isQuotaExceededForIDunlocked(storagePool, iter->first, QuotaLimitType_SIZE,
               usedQuotaGroup, groupLimits, defaultLimits.getDefaultGroupQuotaSize()))
            {
               exceededGIDsSize.push_back(iter->first);
            }

            if (isQuotaExceededForIDunlocked(storagePool, iter->first, QuotaLimitType_INODE,
               usedQuotaGroup, groupLimits, defaultLimits.getDefaultGroupQuotaInodes()))
            {
               exceededGIDsInodes.push_back(iter->first);
            }
         }
      }
   }

   storagePool.exceededQuotaStore.updateExceededQuota(&exceededGIDsSize, QuotaDataType_GROUP,
      QuotaLimitType_SIZE);
   storagePool.exceededQuotaStore.updateExceededQuota(&exceededGIDsInodes, QuotaDataType_GROUP,
      QuotaLimitType_INODE);
}

/**
 * Checks if the quota is exceeded for a single ID, not synchronized.
 *
 * @param storagePool
 * @param id The UID/GID to check.
 * @param limitType The limit type, inode or size.
 * @param usedQuota The quota data store.
 * @param limits The quota limits.
 * @return True if the quota is exceeded for the given ID.
 */
bool QuotaManager::isQuotaExceededForIDunlocked(const StoragePoolEx& storagePool, unsigned id,
   QuotaLimitType limitType, const QuotaDataMapForTarget& usedQuota, const QuotaDataMap& limits,
   size_t defaultLimit)
{
   uint64_t usedValue = 0;
   uint64_t limitValue = 0;

   for(QuotaDataMapForTargetConstIter iter = usedQuota.begin(); iter != usedQuota.end(); iter++)
   {
      // filter by target
      if (! storagePool.hasTarget(iter->first))
         continue;

      QuotaDataMapConstIter quotaDataIter = iter->second.find(id);
      if(quotaDataIter != iter->second.end() )
      {
         QuotaData quotaDataUsed = quotaDataIter->second;

         if(limitType == QuotaLimitType_SIZE)
            usedValue += quotaDataUsed.getSize();
         else if(limitType == QuotaLimitType_INODE)
            usedValue += quotaDataUsed.getInodes();
      }
   }

   QuotaDataMapConstIter quotaLimitsIter = limits.find(id);
   if(quotaLimitsIter != limits.end() )
   {
      if(limitType == QuotaLimitType_SIZE)
         limitValue = quotaLimitsIter->second.getSize();
      else if(limitType == QuotaLimitType_INODE)
         limitValue = quotaLimitsIter->second.getInodes();
   }


   if(limitValue != 0)
      return limitValue <= usedValue;
   else
      return (defaultLimit != 0) && (defaultLimit <= usedValue);
}

/**
 * Transfers the exceeded quota IDs for all storage pools to the storage servers.
 */
void QuotaManager::pushExceededQuotaIDs()
{
   StoragePoolExPtrVec storagePools = storagePoolStore->getPoolsAsVec();

   for (auto iter = storagePools.begin(); iter != storagePools.end(); iter++)
   {
      pushExceededQuotaIDs(**iter);
   }
}

/**
 * Transfers the exceeded quota IDs for one storage pool to the storage servers.
 */
void QuotaManager::pushExceededQuotaIDs(const StoragePoolEx& storagePool)
{
   App* app = Program::getApp();

   auto storageNodeList = app->getStorageNodes()->referenceAllNodes();
   auto metadataNodeList = app->getMetaNodes()->referenceAllNodes();

   const ExceededQuotaStore* exQuotaStore = storagePool.getExceededQuotaStore();
   StoragePoolId poolId = storagePool.getId();

   int numWorks = 0;
   SynchronizedCounter counter;

   IntVector nodeResults;

   UIntList exceededQuotaUIDSize;
   exQuotaStore->getExceededQuota(&exceededQuotaUIDSize, QuotaDataType_USER,
      QuotaLimitType_SIZE);
   int maxMsgCountUIDSize = getMaxMessageCountForPushExceededQuota(&exceededQuotaUIDSize);

   UIntList exceededQuotaUIDInodes;
   exQuotaStore->getExceededQuota(&exceededQuotaUIDInodes, QuotaDataType_USER,
      QuotaLimitType_INODE);
   int maxMsgCountUIDInodes = getMaxMessageCountForPushExceededQuota(&exceededQuotaUIDInodes);

   UIntList exceededQuotaGIDSize;
   exQuotaStore->getExceededQuota(&exceededQuotaGIDSize, QuotaDataType_GROUP,
      QuotaLimitType_SIZE);
   int maxMsgCountGIDSize = getMaxMessageCountForPushExceededQuota(&exceededQuotaGIDSize);

   UIntList exceededQuotaGIDInodes;
   exQuotaStore->getExceededQuota(&exceededQuotaGIDInodes, QuotaDataType_GROUP,
      QuotaLimitType_INODE);
   int maxMsgCountGIDInodes = getMaxMessageCountForPushExceededQuota(&exceededQuotaGIDInodes);

   // use IntVector because it doesn't work with BoolVector. std::vector<bool> is not a container
   nodeResults = IntVector( (maxMsgCountUIDSize + maxMsgCountUIDInodes + maxMsgCountGIDSize +
      maxMsgCountGIDInodes) * (storageNodeList.size() + metadataNodeList.size() ) );


   // upload all subranges (=> msg size limitation) for user size limits to the storage servers
   for(int messageNumber = 0; messageNumber < maxMsgCountUIDSize; messageNumber++)
   {
      for (auto nodeIter = storageNodeList.begin(); nodeIter != storageNodeList.end(); nodeIter++)
      {
         Work* work = new SetExceededQuotaWork(poolId, QuotaDataType_USER, QuotaLimitType_SIZE,
            **nodeIter, messageNumber, &exceededQuotaUIDSize, &counter, &nodeResults[numWorks]);
         workQueue.addDirectWork(work);

         numWorks++;
      }
   }

   // upload all subranges (=> msg size limitation) for user size limits to the metadata servers
   for(int messageNumber = 0; messageNumber < maxMsgCountUIDSize; messageNumber++)
   {
      for (auto nodeIter = metadataNodeList.begin(); nodeIter != metadataNodeList.end(); nodeIter++)
      {
         Work* work = new SetExceededQuotaWork(poolId, QuotaDataType_USER, QuotaLimitType_SIZE,
            **nodeIter, messageNumber, &exceededQuotaUIDSize, &counter, &nodeResults[numWorks]);
         workQueue.addDirectWork(work);

         numWorks++;
      }
   }

   // upload all subranges (=> msg size limitation) for user inode limits to the storage servers
   for(int messageNumber = 0; messageNumber < maxMsgCountUIDInodes; messageNumber++)
   {
      for (auto nodeIter = storageNodeList.begin(); nodeIter != storageNodeList.end(); nodeIter++)
      {
         Work* work = new SetExceededQuotaWork(poolId, QuotaDataType_USER, QuotaLimitType_INODE,
            **nodeIter, messageNumber, &exceededQuotaUIDInodes, &counter, &nodeResults[numWorks]);
         workQueue.addDirectWork(work);

         numWorks++;
      }
   }

   // upload all subranges (=> msg size limitation) for user inode limits to the metadata servers
   for(int messageNumber = 0; messageNumber < maxMsgCountUIDInodes; messageNumber++)
   {
      for (auto nodeIter = metadataNodeList.begin(); nodeIter != metadataNodeList.end(); nodeIter++)
      {
         Work* work = new SetExceededQuotaWork(poolId, QuotaDataType_USER, QuotaLimitType_INODE,
            **nodeIter, messageNumber, &exceededQuotaUIDInodes, &counter, &nodeResults[numWorks]);
         workQueue.addDirectWork(work);

         numWorks++;
      }
   }

   // upload all subranges (=> msg size limitation) for group size limits to the storage servers
   for(int messageNumber = 0; messageNumber < maxMsgCountGIDSize; messageNumber++)
   {
      for (auto nodeIter = storageNodeList.begin(); nodeIter != storageNodeList.end(); nodeIter++)
      {
         Work* work = new SetExceededQuotaWork(poolId, QuotaDataType_GROUP, QuotaLimitType_SIZE,
            **nodeIter, messageNumber, &exceededQuotaGIDSize, &counter, &nodeResults[numWorks]);
         workQueue.addDirectWork(work);

         numWorks++;
      }
   }

   // upload all subranges (=> msg size limitation) for group size limits to the metadata servers
   for(int messageNumber = 0; messageNumber < maxMsgCountGIDSize; messageNumber++)
   {
      for (auto nodeIter = metadataNodeList.begin(); nodeIter != metadataNodeList.end(); nodeIter++)
      {
         Work* work = new SetExceededQuotaWork(poolId, QuotaDataType_GROUP, QuotaLimitType_SIZE,
            **nodeIter, messageNumber, &exceededQuotaGIDSize, &counter, &nodeResults[numWorks]);
         workQueue.addDirectWork(work);

         numWorks++;
      }
   }

   // upload all subranges (=> msg size limitation) for group inode limits to the storage servers
   for(int messageNumber = 0; messageNumber < maxMsgCountGIDInodes; messageNumber++)
   {
      for (auto nodeIter = storageNodeList.begin(); nodeIter != storageNodeList.end(); nodeIter++)
      {
         Work* work = new SetExceededQuotaWork(poolId, QuotaDataType_GROUP, QuotaLimitType_INODE,
            **nodeIter, messageNumber, &exceededQuotaGIDInodes, &counter, &nodeResults[numWorks]);
         workQueue.addDirectWork(work);

         numWorks++;
      }
   }

   // upload all subranges (=> msg size limitation) for group inode limits to the metadata servers
   for(int messageNumber = 0; messageNumber < maxMsgCountGIDInodes; messageNumber++)
   {
      for (auto nodeIter = metadataNodeList.begin(); nodeIter != metadataNodeList.end(); nodeIter++)
      {
         Work* work = new SetExceededQuotaWork(poolId, QuotaDataType_GROUP, QuotaLimitType_INODE,
            **nodeIter, messageNumber, &exceededQuotaGIDInodes, &counter, &nodeResults[numWorks]);
         workQueue.addDirectWork(work);

         numWorks++;
      }
   }

   // wait for all work to be done
   counter.waitForCount(numWorks);
}

/**
 * Calculate the number of massages which are required to upload all exceeded quota IDs.
 *
 * @param listToSend The list with the IDs to send.
 */
int QuotaManager::getMaxMessageCountForPushExceededQuota(UIntList* listToSend)
{
   int retVal = listToSend->size() / SETEXCEEDEDQUOTAMSG_MAX_ID_COUNT;

   if( (listToSend->size() % SETEXCEEDEDQUOTAMSG_MAX_ID_COUNT) != 0)
      retVal++;

   // at least on on message is required to reset the exceeded quota IDs
   if(retVal == 0)
      retVal++;

   return retVal;
}

/**
 * Saves the quota data on the hard-drives.
 */
void QuotaManager::saveQuotaData()
{
   // save quota limits for all pools
   StoragePoolExPtrVec storagePools = storagePoolStore->getPoolsAsVec();
   for (auto iter = storagePools.begin(); iter != storagePools.end(); iter++)
   {
      (*iter)->saveQuotaLimits();
   }

   {
      RWLockGuard const lock(usedQuotaUserRWLock, SafeRWLock_READ);

      QuotaData::saveQuotaDataMapForTargetToFile(usedQuotaUser, USED_QUOTA_USER_PATH);
   }


   {
      RWLockGuard const lock(usedQuotaGroupRWLock, SafeRWLock_READ);

      QuotaData::saveQuotaDataMapForTargetToFile(this->usedQuotaGroup, USED_QUOTA_GROUP_PATH);
   }
}

/**
 * Loads the quota data from the hard-drives.
 */
void QuotaManager::loadQuotaData()
{
   const std::string mgmtStorePath = Program::getApp()->getConfig()->getStoreMgmtdDirectory();

   // load quota limits for all pools
   StoragePoolExPtrVec storagePools = storagePoolStore->getPoolsAsVec();
   for (auto iter = storagePools.begin(); iter != storagePools.end(); iter++)
   {
      (*iter)->loadQuotaLimits();
   }

   {
      RWLockGuard const lock(usedQuotaUserRWLock, SafeRWLock_WRITE);

      Path usedQuotaUserPath(mgmtStorePath + "/" + USED_QUOTA_USER_PATH);
      if(QuotaData::loadQuotaDataMapForTargetFromFile(this->usedQuotaUser,
         usedQuotaUserPath.str() ) )
      {
         log.log(Log_NOTICE, "Loaded quota data for users of " +
            StringTk::intToStr(this->usedQuotaUser.size()) + " targets from file " +
            usedQuotaUserPath.str() );
      }
   }

   {
      RWLockGuard const lock(usedQuotaGroupRWLock, SafeRWLock_WRITE);

      Path usedQuotaGroupPath(mgmtStorePath + "/" + USED_QUOTA_GROUP_PATH);
      if(QuotaData::loadQuotaDataMapForTargetFromFile(this->usedQuotaGroup,
         usedQuotaGroupPath.str() ) )
      {
         log.log(Log_NOTICE, "Loaded quota data for groups of " +
            StringTk::intToStr(this->usedQuotaGroup.size() ) + " targets from file " +
            usedQuotaGroupPath.str() );
      }
   }
}

/**
 * Returns the quota limit of a given id.
 *
 * @param inOutQuotaData The input and output QuotaData for this method, the ID and the type must
 *        set before the method was called, the method updates the size and inode value of this
 *        QuotaData object with the limits for the given ID.
 * @param storagePoolId the ID of the storage pool to query limits for
 *
 * @return true if limits were found.
 */
bool QuotaManager::getQuotaLimitsForID(StoragePoolId storagePoolId,
   QuotaData& inOutQuotaData) const
{
   // get the pool
   StoragePoolExPtr storagePool = storagePoolStore->getPool(storagePoolId);
   if (!storagePool) // invalid pool ID
      return false;

   if(inOutQuotaData.getType() == QuotaDataType_USER)
      return storagePool->quotaUserLimits->getQuotaLimit(inOutQuotaData);
   else
   if(inOutQuotaData.getType() == QuotaDataType_GROUP)
      return storagePool->quotaGroupLimits->getQuotaLimit(inOutQuotaData);

   return false;
}

/**
 * Returns the quota limit for every user ID in the range.
 *
 * @param rangeStart Lowest UID in range.
 * @param rangeEnd Highest UID in range (inclusive).
 * @param type The quota limit type.
 * @param storagePoolId
 * @param outQuotaDataList Quota limits are appended to this list.
 * @return True if a limit was found.
 */
bool QuotaManager::getQuotaLimitsForRange(unsigned rangeStart, unsigned rangeEnd,
   QuotaDataType type, StoragePoolId storagePoolId, QuotaDataList& outQuotaDataList) const
{
   // get the pool
   StoragePoolExPtr storagePool = storagePoolStore->getPool(storagePoolId);
   if (!storagePool) // invalid pool ID
      return false;

   if (type == QuotaDataType_USER)
   {
      return storagePool->quotaUserLimits->getQuotaLimitForRange(rangeStart, rangeEnd,
         outQuotaDataList);
   }
   else if (type == QuotaDataType_GROUP)
   {
      return storagePool->quotaGroupLimits->getQuotaLimitForRange(rangeStart, rangeEnd,
         outQuotaDataList);
   }

   return false;
}

/**
 * Getter for the default quota limits.
 *
 * @param storagePoolId
 * @param outLimits the limits itself, if loading was successful
 *
 * @return true if limits could be loaded, false otherwise
 */
bool QuotaManager::getDefaultLimits(StoragePoolId storagePoolId,
      QuotaDefaultLimits& outLimits) const
{
   // get the pool
   StoragePoolExPtr storagePool = storagePoolStore->getPool(storagePoolId);
   if (!storagePool) // invalid pool ID
   {
      LOG(QUOTA, ERR, "Unable to get default quota information, "
               "because given storage pool ID doesn't exist",
               storagePoolId);
      return false;
   }

   outLimits = *(storagePool->quotaDefaultLimits);

   return true;
}

/**
 * Creates a string which contains all used quota data of the users. The string could be printed to
 * the console.
 *
 * NOTE: It is just used by the GenericDebugMsg.
 *
 *@param mergeTargets True if the quota data of all targets should be merged then for each ID only
 *       one used quota value is added to the string. If false for each target the ID with the used
 *       quota value is added to the string.
 * @return A string which contains all used quota.
 */
std::string QuotaManager::usedQuotaUserToString(bool mergeTargets) const
{
   std::ostringstream returnStringStream;

   RWLockGuard const lock(usedQuotaUserRWLock, SafeRWLock_READ);

   if(mergeTargets)
   {
      //merge the quota data from the targets
      QuotaDataMap mergedQuota;

      for (auto iter = usedQuotaUser.begin(); iter != usedQuotaUser.end(); iter++)
      {
         for (auto idIter = iter->second.begin(); idIter != iter->second.end(); idIter++)
         {
            QuotaDataMapIter foundIter = mergedQuota.find(idIter->first);
            if (foundIter != mergedQuota.end() )
               foundIter->second.mergeQuotaDataCounter(&idIter->second);
            else
               mergedQuota.insert(*idIter);
         }
      }
      QuotaData::quotaDataMapToString(mergedQuota, QuotaDataType_USER, &returnStringStream);
   }
   else
      QuotaData::quotaDataMapForTargetToString(this->usedQuotaUser, QuotaDataType_USER,
         &returnStringStream);

   return returnStringStream.str();
}

/**
 * Creates a string which contains all used quota data of the groups. The string could be printed to
 * the console.
 *
 * NOTE: It is just used by the GenericDebugMsg.
 *
 * @param mergeTargets True if the quota data of all targets should be merged then for each ID only
 *        one used quota value is added to the string. If false for each target the ID with the used
 *        quota value is added to the string.
 * @return A string which contains all used quota.
 */
std::string QuotaManager::usedQuotaGroupToString(bool mergeTargets) const
{
   std::ostringstream returnStringStream;

   RWLockGuard const lock(usedQuotaGroupRWLock, SafeRWLock_READ);

   if(mergeTargets)
   {
      //merge the quota data from the targets
      QuotaDataMap mergedQuota;

      for (auto iter = usedQuotaGroup.begin(); iter != usedQuotaGroup.end(); iter++)
      {
         for (auto idIter = iter->second.begin(); idIter != iter->second.end(); idIter++)
         {
            QuotaDataMapIter foundIter = mergedQuota.find(idIter->first);
            if (foundIter != mergedQuota.end() )
               foundIter->second.mergeQuotaDataCounter(&idIter->second);
            else
               mergedQuota.insert(*idIter);
         }
      }
      QuotaData::quotaDataMapToString(mergedQuota, QuotaDataType_GROUP, &returnStringStream);
   }
   else
      QuotaData::quotaDataMapForTargetToString(this->usedQuotaGroup, QuotaDataType_GROUP,
         &returnStringStream);

   return returnStringStream.str();
}

/**
 * Loads UIDs or GIDs from a file. In case of error it does some retries.
 *
 * NOTE: Normally use this function instead of loadIdsFromFile() because it is allowed to change the
 * ID file during a running system. So error can happen, so we should do a re-try to check if it is
 * a temporary problem.
 *
 * @param path The file to read from.
 * @param numRetries The number of retries to do, if a error happens.
 * @param outIdList The list which contains all IDs from the file afterwards.
 * @return True if the IDs could load successful from the file.
 */
bool QuotaManager::loadIdsFromFileWithReTry(const std::string& path, unsigned numRetries,
   UIntList& outIdList)
{
   bool retVal = false;

   retVal = loadIdsFromFile(path, outIdList);
   while(!retVal && (numRetries > 0) )
   {
      retVal = loadIdsFromFile(path, outIdList);
      --numRetries;
   }

   return retVal;
}

/**
 * Loads UIDs or GIDs from a file.
 *
 * NOTE: Normally use the function loadIdsFromFileWithReTry() instead of this because it is allowed
 * to change the ID file during a running system. So error can happen, so we should do a re-try to
 * check if it is a temporary problem.
 *
 * @param path The file to read from.
 * @param outIdList The list which contains all IDs from the file afterwards.
 * @return True if the IDs could load successful from the file.
 */
bool QuotaManager::loadIdsFromFile(const std::string& path, UIntList& outIdList)
{
   LogContext log("LoadIDsFromFile");

   bool retVal = false;

   if(!path.length() )
      return false;

   std::ifstream reader(path.c_str(), std::ifstream::in);

   if(reader.is_open() )
   {
      unsigned newId;
      reader >> newId;

      while(reader.good() )
      {
         outIdList.push_back(newId);
         reader >> newId;
      }

      if(reader.eof() )
      {  // add the last ID from the file
         outIdList.push_back(newId);
         retVal = true;
      }
      else
      {
         log.log(Log_ERR, "Error during read file with UIDs or GIDs: " + path + ". " +
            "SysErr: " + System::getErrString() );
         retVal = false;
      }

      reader.close();
   }
   else
   {
      log.log(Log_ERR, "Unable to open file with UIDs or GIDs: " + path + ". " +
         "SysErr: " + System::getErrString() );
      retVal = false;
   }

   if(retVal)
   { // remove duplicated IDs
      outIdList.sort();
      outIdList.unique();
   }
   else
   { // in case of error clear the output list
      outIdList.clear();
   }

   return retVal;
}
