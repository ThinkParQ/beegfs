#ifndef CONFIG_H_
#define CONFIG_H_

#include <common/app/config/AbstractConfig.h>


#define CONFIG_CLIENTNODES_FILENAME               "clients.nodes"
#define CONFIG_METANODES_FILENAME                 "meta.nodes"
#define CONFIG_STORAGENODES_FILENAME              "storage.nodes"
#define CONFIG_TARGETMAPPINGS_FILENAME            "targets"
#define CONFIG_STORAGEPOOLS_FILENAME              "storagePools"
#define CONFIG_TARGETNUMIDS_FILENAME              "targetNumIDs"
#define CONFIG_BUDDYGROUPMAPPINGS_FILENAME_OLD    "buddygroups"
#define CONFIG_STORAGEBUDDYGROUPMAPPINGS_FILENAME "storagebuddygroups"
#define CONFIG_METABUDDYGROUPMAPPINGS_FILENAME    "metabuddygroups"
#define CONFIG_STORAGETARGETSTORESYNC_FILENAME    "targetsToResync"
#define CONFIG_METANODESTORESYNC_FILENAME         "nodesToResync"
#define CONFIG_STORAGETARGETSTATES_FILENAME       "targetStates"
#define CONFIG_METANODESTATES_FILENAME            "nodeStates"

#define MGMT_QUOTA_QUERY_TYPE_FILE_STR        "file"
#define MGMT_QUOTA_QUERY_TYPE_RANGE_STR       "range"
#define MGMT_QUOTA_QUERY_TYPE_SYSTEM_STR      "system"


enum MgmtQuotaQueryType
{
   MgmtQuotaQueryType_SYSTEM = 0,
   MgmtQuotaQueryType_RANGE = 1,
   MgmtQuotaQueryType_FILE = 2,
};



class Config : public AbstractConfig
{
   public:
      Config(int argc, char** argv);
   
   private:

      // configurables

      std::string connInterfacesFile; // implicitly generates connInterfacesList
      std::string connInterfacesList; // comma-separated list

      std::string storeMgmtdDirectory;
      bool        storeAllowFirstRunInit;

      unsigned    tuneNumWorkers;
      unsigned    tuneNumQuotaWorkers;
      unsigned    tuneMetaNodeAutoRemoveMins; // ignored! (only for compatibility with old cfg file)
      unsigned    tuneStorageNodeAutoRemoveMins;  // ignored! (only for compat with old cfg file)
      unsigned    tuneClientAutoRemoveMins;

      int64_t     tuneMetaSpaceLowLimit;
      int64_t     tuneMetaSpaceEmergencyLimit;
      int64_t     tuneMetaInodesLowLimit;
      int64_t     tuneMetaInodesEmergencyLimit;
      bool        tuneMetaDynamicPools;

      int64_t     tuneMetaSpaceNormalSpreadThreshold; // only for dynamic pools
      int64_t     tuneMetaSpaceLowSpreadThreshold; // only for dynamic pools
      int64_t     tuneMetaSpaceLowDynamicLimit; // only for dynamic pools
      int64_t     tuneMetaSpaceEmergencyDynamicLimit; // only for dynamic pools
      int64_t     tuneMetaInodesNormalSpreadThreshold; // only for dynamic pools
      int64_t     tuneMetaInodesLowSpreadThreshold; // only for dynamic pools
      int64_t     tuneMetaInodesLowDynamicLimit; // only for dynamic pools
      int64_t     tuneMetaInodesEmergencyDynamicLimit; // only for dynamic pools

      int64_t     tuneStorageSpaceLowLimit;
      int64_t     tuneStorageSpaceEmergencyLimit;
      int64_t     tuneStorageInodesLowLimit;
      int64_t     tuneStorageInodesEmergencyLimit;
      bool        tuneStorageDynamicPools;

      int64_t     tuneStorageSpaceNormalSpreadThreshold; // only for dynamic pools
      int64_t     tuneStorageSpaceLowSpreadThreshold; // only for dynamic pools
      int64_t     tuneStorageSpaceLowDynamicLimit; // only for dynamic pools
      int64_t     tuneStorageSpaceEmergencyDynamicLimit; // only for dynamic pools
      int64_t     tuneStorageInodesNormalSpreadThreshold; // only for dynamic pools
      int64_t     tuneStorageInodesLowSpreadThreshold; // only for dynamic pools
      int64_t     tuneStorageInodesLowDynamicLimit; // only for dynamic pools
      int64_t     tuneStorageInodesEmergencyDynamicLimit; // only for dynamic pools

      unsigned    tuneProcessFDLimit; // 0 means "don't touch limit"

      bool        sysAllowNewServers;
      bool        sysAllowNewTargets;
      unsigned    sysTargetOfflineTimeoutSecs;

      bool        runDaemonized;

      std::string pidFile;

      bool                 quotaEnableEnforcement;
      unsigned             quotaUpdateIntervalMin;
      unsigned             quotaStoreIntervalMin;
      std::string          quotaQueryType;
      MgmtQuotaQueryType   quotaQueryTypeNum;       // auto-generated based on quotaQueryType
      std::string          quotaQueryUIDFile;
      std::string          quotaQueryUIDRange;
      unsigned             quotaQueryUIDRangeStart; // auto-generated based on quotaQueryUIDRange
      unsigned             quotaQueryUIDRangeEnd;   // auto-generated based on quotaQueryUIDRange
      std::string          quotaQueryGIDFile;
      std::string          quotaQueryGIDRange;
      unsigned             quotaQueryGIDRangeStart; // auto-generated based on quotaQueryGIDRange
      unsigned             quotaQueryGIDRangeEnd;   // auto-generated based on quotaQueryGIDRange
      bool                 quotaQueryWithSystemUsersGroups;


      // internals

      virtual void loadDefaults(bool addDashes) override;
      virtual void applyConfigMap(bool enableException, bool addDashes) override;
      virtual void initImplicitVals() override;
      void initQuotaQueryTypeNum();
      void initQuotaQueryRange();
      void initDynamicPoolsLimits();
      std::string createDefaultCfgFilename() const;
      
   public:
      // getters & setters
      const std::string& getConnInterfacesList() const
      {
         return connInterfacesList;
      }

      const std::string& getStoreMgmtdDirectory() const
      {
         return storeMgmtdDirectory;
      }
      
      bool getStoreAllowFirstRunInit() const
      {
         return storeAllowFirstRunInit;
      }

      unsigned getTuneNumWorkers() const
      {
         return tuneNumWorkers;
      }

      unsigned getTuneNumQuotaWorkers() const
      {
         return tuneNumQuotaWorkers;
      }

      unsigned getTuneClientAutoRemoveMins() const
      {
         return tuneClientAutoRemoveMins;
      }

      int64_t getTuneMetaSpaceLowLimit() const
      {
         return tuneMetaSpaceLowLimit;
      }

      int64_t getTuneMetaSpaceEmergencyLimit() const
      {
         return tuneMetaSpaceEmergencyLimit;
      }

      bool getTuneMetaDynamicPools() const
      {
         return tuneMetaDynamicPools;
      }

      int64_t getTuneMetaSpaceNormalSpreadThreshold() const
      {
         return tuneMetaSpaceNormalSpreadThreshold;
      }

      int64_t getTuneMetaSpaceLowSpreadThreshold() const
      {
         return tuneMetaSpaceLowSpreadThreshold;
      }

      int64_t getTuneMetaSpaceLowDynamicLimit() const
      {
         return tuneMetaSpaceLowDynamicLimit;
      }

      int64_t getTuneMetaSpaceEmergencyDynamicLimit() const
      {
         return tuneMetaSpaceEmergencyDynamicLimit;
      }

      int64_t getTuneMetaInodesLowLimit() const
      {
         return tuneMetaInodesLowLimit;
      }

      int64_t getTuneMetaInodesEmergencyLimit() const
      {
         return tuneMetaInodesEmergencyLimit;
      }

      int64_t getTuneMetaInodesNormalSpreadThreshold() const
      {
         return tuneMetaInodesNormalSpreadThreshold;
      }

      int64_t getTuneMetaInodesLowSpreadThreshold() const
      {
         return tuneMetaInodesLowSpreadThreshold;
      }

      int64_t getTuneMetaInodesLowDynamicLimit() const
      {
         return tuneMetaInodesLowDynamicLimit;
      }

      int64_t getTuneMetaInodesEmergencyDynamicLimit() const
      {
         return tuneMetaInodesEmergencyDynamicLimit;
      }

      int64_t getTuneStorageSpaceLowLimit() const
      {
         return tuneStorageSpaceLowLimit;
      }

      int64_t getTuneStorageSpaceEmergencyLimit() const
      {
         return tuneStorageSpaceEmergencyLimit;
      }

      bool getTuneStorageDynamicPools() const
      {
         return tuneStorageDynamicPools;
      }

      int64_t getTuneStorageSpaceNormalSpreadThreshold() const
      {
         return tuneStorageSpaceNormalSpreadThreshold;
      }

      int64_t getTuneStorageSpaceLowSpreadThreshold() const
      {
         return tuneStorageSpaceLowSpreadThreshold;
      }

      int64_t getTuneStorageSpaceLowDynamicLimit() const
      {
         return tuneStorageSpaceLowDynamicLimit;
      }

      int64_t getTuneStorageSpaceEmergencyDynamicLimit() const
      {
         return tuneStorageSpaceEmergencyDynamicLimit;
      }

      int64_t getTuneStorageInodesLowLimit() const
      {
         return tuneStorageInodesLowLimit;
      }

      int64_t getTuneStorageInodesEmergencyLimit() const
      {
         return tuneStorageInodesEmergencyLimit;
      }

      int64_t getTuneStorageInodesNormalSpreadThreshold() const
      {
         return tuneStorageInodesNormalSpreadThreshold;
      }

      int64_t getTuneStorageInodesLowSpreadThreshold() const
      {
         return tuneStorageInodesLowSpreadThreshold;
      }

      int64_t getTuneStorageInodesLowDynamicLimit() const
      {
         return tuneStorageInodesLowDynamicLimit;
      }

      int64_t getTuneStorageInodesEmergencyDynamicLimit() const
      {
         return tuneStorageInodesEmergencyDynamicLimit;
      }

      unsigned getTuneProcessFDLimit() const
      {
         return tuneProcessFDLimit;
      }

      bool getSysAllowNewServers() const
      {
         return sysAllowNewServers;
      }

      bool getSysAllowNewTargets() const
      {
         return sysAllowNewTargets;
      }

      unsigned getSysTargetOfflineTimeoutSecs() const
      {
         return sysTargetOfflineTimeoutSecs;
      }

      bool getRunDaemonized() const
      {
         return runDaemonized;
      }

      const std::string& getPIDFile() const
      {
         return pidFile;
      }

      bool getQuotaEnableEnforcement() const
      {
         return quotaEnableEnforcement;
      }

      unsigned getQuotaUpdateIntervalMin() const
      {
         return quotaUpdateIntervalMin;
      }

      unsigned getQuotaStoreIntervalMin() const
      {
         return quotaStoreIntervalMin;
      }

      MgmtQuotaQueryType getQuotaQueryTypeNum() const
      {
         return quotaQueryTypeNum;
      }

      unsigned getQuotaQueryUIDRangeStart() const
      {
         return quotaQueryUIDRangeStart;
      }

      unsigned getQuotaQueryUIDRangeEnd() const
      {
         return quotaQueryUIDRangeEnd;
      }

      unsigned getQuotaQueryGIDRangeStart() const
      {
         return quotaQueryGIDRangeStart;
      }

      unsigned getQuotaQueryGIDRangeEnd() const
      {
         return quotaQueryGIDRangeEnd;
      }

      bool getQuotaQueryWithSystemUsersGroups() const
      {
         return quotaQueryWithSystemUsersGroups;
      }

      const std::string& getQuotaQueryGIDFile() const
      {
         return quotaQueryGIDFile;
      }

      const std::string& getQuotaQueryUIDFile() const
      {
         return quotaQueryUIDFile;
      }
};

#endif /*CONFIG_H_*/
