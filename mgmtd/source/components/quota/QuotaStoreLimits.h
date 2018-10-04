#ifndef QUOTASTORELIMITS_H_
#define QUOTASTORELIMITS_H_

#include <common/threading/RWLock.h>
#include <common/threading/RWLockGuard.h>
#include <common/storage/quota/QuotaData.h>
#include <common/Common.h>



class QuotaStoreLimits
{
   public:
      QuotaStoreLimits() { };

      QuotaStoreLimits(QuotaDataType type, const std::string logContext,
         const std::string storePath) :
#ifdef BEEGFS_DEBUG
         quotaType(type),
#endif
         logContext(logContext), storePath(storePath)
      {
         IGNORE_UNUSED_DEBUG_VARIABLE(type);
      }

      bool loadFromFile();
      bool saveToFile();

      void clearLimits();

      void addOrUpdateLimit(const QuotaData quotaData);
      void addOrUpdateLimits(const QuotaDataList& quotaDataList);

      bool getQuotaLimit(QuotaData& quotaDataInOut);
      bool getQuotaLimitForRange(const unsigned rangeStart, const unsigned rangeEnd,
         QuotaDataList& outQuotaDataList);

      QuotaDataMap getAllQuotaLimits();

   private:
#ifdef BEEGFS_DEBUG
      QuotaDataType quotaType; // type of allowed QuotaData for the map, used only for debug
#endif

      std::string logContext; // the LogContext string, required for multiple use in the same daemon

      RWLock limitsRWLock; // syncs access to the limits

      std::string storePath; // not thread-safe!

      QuotaDataMap limits; // the individual quota limits

      void addOrUpdateLimitUnlocked(const QuotaData quotaData);

   public:
      // getters & setters

      std::string getStorePath()
      {
         return this->storePath;
      }

      /**
       * Get number of stored quota limits.
       */
      size_t getSize()
      {
         RWLockGuard const lock(limitsRWLock, SafeRWLock_READ);

         return limits.size();
      }
};

#endif /* QUOTASTORELIMITS_H_ */
