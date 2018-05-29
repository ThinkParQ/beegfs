#ifndef MGMTD_STORAGEPOOLEX_H
#define MGMTD_STORAGEPOOLEX_H

#include <common/storage/quota/ExceededQuotaStore.h>
#include <common/storage/StoragePool.h>
#include <components/quota/QuotaStoreLimits.h>
#include <program/Program.h>

#define USER_LIMITS_STORE_PATH(idStr) \
      Program::getApp()->getConfig()->getStoreMgmtdDirectory() + \
      "/" + QuotaManager::QUOTA_SAVE_DIR + \
      "/" + idStr + \
      "/" + QuotaManager::USER_LIMITS_FILENAME

#define GROUP_LIMITS_STORE_PATH(idStr) \
      Program::getApp()->getConfig()->getStoreMgmtdDirectory() + \
      "/" + QuotaManager::QUOTA_SAVE_DIR + \
      "/" + idStr + \
      "/" + QuotaManager::GROUP_LIMITS_FILENAME

#define DEFAULT_LIMITS_STORE_PATH(idStr) \
      Program::getApp()->getConfig()->getStoreMgmtdDirectory() + \
      "/" + QuotaManager::QUOTA_SAVE_DIR + \
      "/" + idStr + \
      "/" + QuotaManager::DEFAULT_LIMITS_FILENAME

/*
 * a storage target pool with an attached quota manager, only used in mgmtd
 */
class StoragePoolEx: public StoragePool
{
   friend class QuotaManager;

   public:
      StoragePoolEx(StoragePoolId id, const std::string& description,
         const DynamicPoolLimits& capPoolLimitsStorageSpace,
         const DynamicPoolLimits& capPoolLimitsStorageInodes, bool useDynamicCapPools);

      // use this only for deserialization and don't forget to call initQuota after deserialization
      StoragePoolEx(const DynamicPoolLimits& capPoolLimitsStorageSpace,
         const DynamicPoolLimits& capPoolLimitsStorageInodes, bool useDynamicCapPools) :
         StoragePool(StoragePoolId(0), "", capPoolLimitsStorageSpace, capPoolLimitsStorageInodes,
                     useDynamicCapPools)
      { };

      void initQuota();
      void clearQuota();

      bool initFromDesBuf(Deserializer& des) override;

      const ExceededQuotaStore* getExceededQuotaStore() const
      {
         return &exceededQuotaStore;
      }

   private:
      std::unique_ptr<QuotaStoreLimits> quotaUserLimits;
      std::unique_ptr<QuotaStoreLimits> quotaGroupLimits;
      std::unique_ptr<QuotaDefaultLimits> quotaDefaultLimits;
      AtomicInt16 quotaLimitChanged;

      ExceededQuotaStore exceededQuotaStore;

      void saveQuotaLimits();
      void loadQuotaLimits();
};

#endif /*MGMTD_STORAGEPOOLEX_H*/
