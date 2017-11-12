#ifndef MODEGETQUOTAINFO_H_
#define MODEGETQUOTAINFO_H_

#include <common/storage/StoragePoolId.h>
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
      NodeStoreServers* storageNodes;
      NodeStoreServers* mgmtNodes;
      TargetMapper* targetMapper;
      StoragePoolStore* storagePoolStore;

      int checkConfig(StringMap* cfg);
      bool queryAndPrintQuota(const NodeHandle& mgmtNode, StoragePoolId storagePoolId,
         bool printHeader);
      bool requestDefaultLimits(Node& mgmtNode, QuotaDefaultLimits& defaultLimits,
         StoragePoolId storagePoolId);
      void printQuota(QuotaDataMap* usedQuota, QuotaDataMap* quotaLimits,
         QuotaDefaultLimits& defaultLimits, QuotaInodeSupport quotaInodeSupport);
      bool printQuotaForID(QuotaDataMap* usedQuota, QuotaDataMap* quotaLimits, unsigned id,
         std::string& defaultSizeLimit, std::string& defaultInodeLimit,
         QuotaInodeSupport quotaInodeSupport);
      bool printQuotaForRange(QuotaDataMap* usedQuota, QuotaDataMap* quotaLimits,
         std::string& defaultSizeLimitValue, std::string& defaultInodeLimit,
         QuotaInodeSupport quotaInodeSupport);
      bool printQuotaForList(QuotaDataMap* usedQuota, QuotaDataMap* quotaLimits,
         std::string& defaultSizeLimitValue, std::string& defaultInodeLimit,
         QuotaInodeSupport quotaInodeSupport);
      void printDefaultQuotaLimits(QuotaDefaultLimits& quotaLimits);

      int getUsedQuotaStrings(QuotaDataMap* usedQuota, unsigned id,
         QuotaInodeSupport quotaInodeSupport, std::string& usedSizeOut, std::string& usedInodesOut);
      void getQuotaLimitsStrings(QuotaDataMap* quotaLimits, unsigned id,
         std::string& defaultSizeLimit, std::string& defaultInodeLimit, std::string& limitSizeOut,
         std::string& limitInodesOut);
      void getDefaultQuotaStrings(QuotaDefaultLimits& quotaLimits, QuotaDataType type,
         std::string& defaultLimitSizeOut, std::string& defaultLimitInodesOut);
      bool isUnlimtedValue(uint64_t inValue);
};

#endif /* MODEGETQUOTAINFO_H_ */
