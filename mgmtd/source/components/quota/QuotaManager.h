#ifndef QUOTAMANAGER_H_
#define QUOTAMANAGER_H_

#include <common/app/log/LogContext.h>
#include <common/components/worker/Worker.h>
#include <common/storage/quota/GetQuotaInfo.h>
#include <common/storage/quota/ExceededQuotaStore.h>
#include <common/storage/quota/QuotaDefaultLimits.h>
#include <common/threading/RWLock.h>
#include <common/Common.h>
#include <components/quota/QuotaDataRequestor.h>
#include <components/quota/QuotaStoreLimits.h>
#include <nodes/StoragePoolStoreEx.h>


/**
 * The quota manager which coordinates all the quota enforcement communication, it starts the
 * slave processes to get the quota data, calculate the exceeded quota IDs and push the exceeded IDs
 * to the servers.
 */
class QuotaManager : public PThread
{
   public:
      QuotaManager();
      ~QuotaManager();

      static const std::string QUOTA_SAVE_DIR;
      static const std::string USER_LIMITS_FILENAME;
      static const std::string GROUP_LIMITS_FILENAME;
      static const std::string DEFAULT_LIMITS_FILENAME;

      void run();
      void updateQuotaLimits(StoragePoolId storagePoolId, const QuotaDataList& list);
      void removeTargetFromQuotaDataStores(uint16_t targetNumID);
      bool getQuotaLimitsForID(StoragePoolId storagePoolId, QuotaData& inOutQuotaData) const;
      bool getQuotaLimitsForRange(unsigned rangeStart, unsigned rangeEnd, QuotaDataType type,
         StoragePoolId storagePoolId, QuotaDataList& outQuotaDataList) const;
      std::string usedQuotaUserToString(bool mergeTargets) const;
      std::string usedQuotaGroupToString(bool mergeTargets) const;


   private:
      static const unsigned READ_ID_FILE_NUM_RETRIES = 3;
      static const std::string USED_QUOTA_USER_PATH; // relative path inside mgmt directory
      static const std::string USED_QUOTA_GROUP_PATH; // relative path inside mgmt directory

      LogContext log;

      StoragePoolStoreEx* storagePoolStore;

      mutable RWLock usedQuotaUserRWLock; // syncs access to the usedQuotaUser
      mutable RWLock usedQuotaGroupRWLock; // syncs access to the usedQuotaGroup

      QuotaDataMapForTarget usedQuotaUser;
      QuotaDataMapForTarget usedQuotaGroup;


      // QuotaManager will have its own workqueue
      MultiWorkQueue workQueue;
      std::list<std::unique_ptr<Worker>> workerList;
      void requestLoop();

      bool updateUsedQuota(UIntList& uidList, UIntList& gidList);
      bool collectQuotaDataFromServer();
      void calculateExceededQuota(StoragePoolEx& storagePool, UIntList& uidList,
         UIntList& gidList);
      bool isQuotaExceededForIDunlocked(const StoragePoolEx& storagePool, unsigned id,
         QuotaLimitType limitType, const QuotaDataMapForTarget& usedQuota,
         const QuotaDataMap& limits, size_t defaultLimit);
      void pushExceededQuotaIDs();
      void pushExceededQuotaIDs(const StoragePoolEx& storagePool);
      int getMaxMessageCountForPushExceededQuota(UIntList* listToSend);

      void saveQuotaData();
      void loadQuotaData();

      bool loadIdsFromFile(const std::string& path, UIntList& outIdList);
      bool loadIdsFromFileWithReTry(const std::string& path, unsigned numRetries,
         UIntList& outIdList);


   public:
      bool updateDefaultUserLimits(StoragePoolId storagePoolId, size_t inodeLimit,
         size_t sizeLimit);
      bool updateDefaultGroupLimits(StoragePoolId storagePoolId, size_t inodeLimit,
         size_t sizeLimit);
      bool getDefaultLimits(StoragePoolId storagePoolId,
         QuotaDefaultLimits& outLimits) const;
};

#endif /* QUOTAMANAGER_H_ */
