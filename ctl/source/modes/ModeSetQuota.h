#ifndef MODESETQUOTA_H_
#define MODESETQUOTA_H_


#include <common/storage/quota/QuotaConfig.h>
#include <common/storage/quota/QuotaData.h>
#include <common/net/message/storage/quota/SetQuotaMsg.h>

#include "Mode.h"



/**
 * A struct for all configurations which are required to set quota limits
 */
struct SetQuotaConfig : public QuotaConfig
{
   bool cfgUseFile;           // true if the quota limits are need to loaded from a file
   uint64_t cfgSizeLimit;     // the quota limit for size
   uint64_t cfgInodeLimit;    // the quota limit for inodes
   std::string cfgFilePath;   // the path to the file with the quota limits
};


class ModeSetQuota : public Mode
{
   public:
      ModeSetQuota() : Mode()
      {
         cfg.cfgType = QuotaDataType_NONE;
         cfg.cfgID = 0;
         cfg.cfgUseAll = false;
         cfg.cfgUseFile = false;
         cfg.cfgUseList = false;
         cfg.cfgUseRange = false;
         cfg.cfgIDRangeStart = 0;
         cfg.cfgIDRangeEnd = 0;
         cfg.cfgSizeLimit = 0;
         cfg.cfgInodeLimit = 0;
         cfg.cfgFilePath = "";
         cfg.cfgDefaultLimits = false;
         cfg.cfgStoragePoolId = StoragePoolId(1); // default ID=1
      }

      virtual int execute();
      static void printHelp();

   protected:

   private:
      SetQuotaConfig cfg;
      QuotaDataList quotaLimitsList;

      void convertIDListToQuotaDataList();
      void convertIDRangeToQuotaDataList();
      int checkConfig(StringMap* cfg);
      int readFromFile(std::string pathToFile, QuotaDataType quotaType);
      bool uploadQuotaLimitsAndCollectResponses(Node& mgmtNode);
      bool uploadDefaultQuotaLimitsAndCollectResponses(Node& mgmtNode);
      int getMaxMessageCount();
      void prepareMessage(int messageNumber, SetQuotaMsg* msg);
};

#endif /* MODESETQUOTA_H_ */
