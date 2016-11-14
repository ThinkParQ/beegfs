#ifndef MODEGETQUOTAINFO_H_
#define MODEGETQUOTAINFO_H_


#include <common/storage/quota/GetQuotaInfo.h>
#include <common/storage/quota/Quota.h>
#include <common/storage/quota/QuotaData.h>
#include <common/storage/quota/QuotaDefaultLimits.h>

#include "Mode.h"



class ModeGetQuotaInfo : public Mode, GetQuotaInfo
{
   public:
      ModeGetQuotaInfo() : Mode(), GetQuotaInfo()
      {
      }

      virtual int execute();
      static void printHelp();

   protected:

   private:
      int checkConfig(StringMap* cfg);

      bool requestDefaultLimits(Node& mgmtNode, QuotaDefaultLimits& defaultLimits);

      void printQuota(QuotaDataMap* usedQuota, QuotaDataMap* quotaLimits,
         QuotaDefaultLimits& defaultLimits, QuotaInodeSupport quotaInodeSupport);
      bool printQuotaForID(QuotaDataMap* usedQuota, QuotaDataMap* quotaLimits, unsigned id,
         std::string& defaultSizeLimitValue, std::string& defaultSizeLimitUnit,
         uint64_t defaultInodeLimit, QuotaInodeSupport quotaInodeSupport);
      bool printQuotaForRange(QuotaDataMap* usedQuota, QuotaDataMap* quotaLimits,
         std::string& defaultSizeLimitValue, std::string& defaultSizeLimitUnit,
         uint64_t defaultInodeLimit, QuotaInodeSupport quotaInodeSupport);
      bool printQuotaForList(QuotaDataMap* usedQuota, QuotaDataMap* quotaLimits,
         std::string& defaultSizeLimitValue, std::string& defaultSizeLimitUnit,
         uint64_t defaultInodeLimit, QuotaInodeSupport quotaInodeSupport);
      void printDefaultQuotaLimits(QuotaDefaultLimits& quotaLimits);
};

#endif /* MODEGETQUOTAINFO_H_ */
