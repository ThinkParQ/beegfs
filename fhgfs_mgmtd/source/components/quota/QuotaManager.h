#ifndef QUOTAMANAGER_H_
#define QUOTAMANAGER_H_

#include <common/app/log/LogContext.h>
#include <common/storage/quota/GetQuotaInfo.h>
#include <common/storage/quota/ExceededQuotaStore.h>
#include <common/storage/quota/QuotaDefaultLimits.h>
#include <common/threading/RWLock.h>
#include <common/Common.h>
#include <components/quota/QuotaDataRequestor.h>
#include <components/quota/QuotaStoreLimits.h>


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

      void run();
      void updateQuotaLimits(const QuotaDataList& list);
      void removeTargetFromQuotaDataStores(uint16_t targetNumID);
      bool getQuotaLimitsForID(QuotaData& inOutQuotaData);
      bool getQuotaLimitsForRange(unsigned rangeStart, unsigned rangeEnd, QuotaDataType type,
         QuotaDataList* outQuotaDataList);
      std::string usedQuotaUserToString(bool mergeTargets);
      std::string usedQuotaGroupToString(bool mergeTargets);


   private:
      LogContext log;

      RWLock usedQuotaUserRWLock; // syncs access to the usedQuotaUser
      RWLock usedQuotaGroupRWLock; // syncs access to the usedQuotaGroup

      QuotaDataMapForTarget *usedQuotaUser;
      QuotaDataMapForTarget *usedQuotaGroup;

      bool usedQuotaUserStoreDirty;
      bool usedQuotaGroupStoreDirty;

      QuotaStoreLimits* quotaUserLimits;
      QuotaStoreLimits* quotaGroupLimits;

      ExceededQuotaStore* exceededQuotaStore;

      QuotaDefaultLimits defaultLimits;

      AtomicInt16 limitChanged;

      void requestLoop();

      bool updateUsedQuota(UIntList& uidList, UIntList& gidList);
      bool collectQuotaDataFromServer();
      void calculateExceededQuota(UIntList& uidList, UIntList& gidList);
      bool isQuotaExceededForIDunlocked(unsigned id, QuotaLimitType limitType,
         QuotaDataMapForTarget* usedQuota, QuotaDataMap* limits, size_t defaultLimit);
      bool pushExceededQuotaIDs();
      int getMaxMessageCountForPushExceededQuota(UIntList* listToSend);

      bool saveQuotaData();
      bool loadQuotaData();

      bool saveDefaultQuotaToFile();
      bool loadDefaultQuotaFromFile();

      bool loadIdsFromFile(const std::string& path, UIntList& outIdList);
      bool loadIdsFromFileWithReTry(const std::string& path, unsigned numRetries,
         UIntList& outIdList);


   public:
      // getters & setters

      /**
       * Getter for the store of the IDs with exceeded quota.
       */
      ExceededQuotaStore* getExceededQuotaStore()
      {
         return this->exceededQuotaStore;
      }

      /**
       * Updates the default user quota limits.
       *
       * @param inodeLimit The new default quota inode limit.
       * @param sizeLimit The new default quota size limit.
       * @return True if the default limits could be stored successful.
       */
      bool updateDefaultUserLimits(size_t inodeLimit, size_t sizeLimit)
      {
         bool retVal = false;

         defaultLimits.updateUserLimits(inodeLimit, sizeLimit);
         retVal = saveDefaultQuotaToFile();

         // the calculation of the exceeded quota is done in the run loop
         // Note: don't do the calculation here because the beegfs-ctl command doesn't finish before
         // the calculation and the update of the server is done in this case
         if(retVal)
            limitChanged.set(1);

         return retVal;
      }

      /**
       * Updates the default group quota limits.
       *
       * @param inodeLimit The new default quota inode limit.
       * @param sizeLimit The new default quota size limit.
       * @return True if the default limits could be stored successful.
       */
      bool updateDefaultGroupLimits(size_t inodeLimit, size_t sizeLimit)
      {
         bool retVal = false;

         defaultLimits.updateGroupLimits(inodeLimit, sizeLimit);
         retVal = saveDefaultQuotaToFile();

         // the calculation of the exceeded quota is done in the run loop
         // Note: don't do the calculation here because the beegfs-ctl command doesn't finish before
         // the calculation and the update of the server is done in this case
         if(retVal)
            limitChanged.set(1);

         return retVal;
      }

      /**
       * Getter for the default quota limits.
       */
      QuotaDefaultLimits getDefaultLimits()
      {
         return defaultLimits;
      }
};

#endif /* QUOTAMANAGER_H_ */
