#ifndef GETQUOTAINFOHELPER_H_
#define GETQUOTAINFOHELPER_H_


#include <common/Common.h>
#include <common/components/worker/queue/MultiWorkQueue.h>
#include <common/net/message/storage/quota/GetQuotaInfoMsg.h>
#include <common/nodes/NodeStoreServers.h>
#include <common/nodes/Node.h>
#include <common/storage/quota/Quota.h>
#include <common/storage/quota/QuotaData.h>
#include <common/storage/quota/GetQuotaConfig.h>



/*
 * A parent class for all classes which collects quota data
 */
class GetQuotaInfo
{
   public:
      GetQuotaInfo()
      {
         cfg.cfgType = QuotaDataType_NONE;
         cfg.cfgID = 0;
         cfg.cfgUseAll = false;
         cfg.cfgUseList = false;
         cfg.cfgUseRange = false;
         cfg.cfgCsv = false;
         cfg.cfgIDRangeStart = 0;
         cfg.cfgIDRangeEnd = 0;
         cfg.cfgTargetSelection = GETQUOTACONFIG_ALL_TARGETS_ONE_REQUEST;
         cfg.cfgTargetNumID = 0;
         cfg.cfgPrintUnused = false;
         cfg.cfgWithSystemUsersGroups = false;
         cfg.cfgDefaultLimits = false;
      }

      bool requestQuotaLimitsAndCollectResponses(const NodeHandle& mgmtNode,
         MultiWorkQueue* workQ, QuotaDataMapForTarget* outQuotaResults, const TargetMapper* mapper,
         StoragePoolId storagePoolId = StoragePoolStore::INVALID_POOL_ID);

      bool requestQuotaDataAndCollectResponses(const NodeStoreServers* storageNodes,
         MultiWorkQueue* workQ, QuotaDataMapForTarget* outQuotaResults, const TargetMapper* mapper,
         QuotaInodeSupport* quotaInodeSupport, StoragePoolStore* storagePoolStore = NULL,
         StoragePoolId storagePoolId = StoragePoolStore::INVALID_POOL_ID);

   protected:
      GetQuotaInfoConfig cfg;

      int getMaxMessageCount();
      QuotaDataMap calculateQuotaSums(const QuotaDataMapForTarget& quotaMaps);
};

#endif /* GETQUOTAINFOHELPER_H_ */
