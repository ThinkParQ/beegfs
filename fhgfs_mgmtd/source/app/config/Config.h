#ifndef CONFIG_H_
#define CONFIG_H_

#include <common/app/config/AbstractConfig.h>


#define CONFIG_CLIENTNODES_FILENAME               "clients.nodes"
#define CONFIG_METANODES_FILENAME                 "meta.nodes"
#define CONFIG_STORAGENODES_FILENAME              "storage.nodes"
#define CONFIG_TARGETMAPPINGS_FILENAME            "targets"
#define CONFIG_TARGETNUMIDS_FILENAME              "targetNumIDs"
#define CONFIG_BUDDYGROUPMAPPINGS_FILENAME_OLD    "buddygroups"
#define CONFIG_STORAGEBUDDYGROUPMAPPINGS_FILENAME "storagebuddygroups"
#define CONFIG_METABUDDYGROUPMAPPINGS_FILENAME    "metabuddygroups"
#define CONFIG_STORAGETARGETSTORESYNC_FILENAME    "targetsToResync"
#define CONFIG_METANODESTORESYNC_FILENAME         "nodesToResync"

#define CONFIG_QUOTA_DATA_DIR                 "quota/"
#define CONFIG_QUOTA_DEFAULT_LIMITS           (CONFIG_QUOTA_DATA_DIR "quotaDefaultLimits.store")
#define CONFIG_QUOTA_USER_LIMITS_FILENAME     (CONFIG_QUOTA_DATA_DIR "quotaUserLimits.store")
#define CONFIG_QUOTA_GROUP_LIMITS_FILENAME    (CONFIG_QUOTA_DATA_DIR "quotaGroupLimits.store")
#define CONFIG_QUOTA_DATA_USER_FILENAME       (CONFIG_QUOTA_DATA_DIR "quotaDataUser.store")
#define CONFIG_QUOTA_DATA_GROUP_FILENAME      (CONFIG_QUOTA_DATA_DIR "quotaDataGroup.store")

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
      Config(int argc, char** argv) throw(InvalidConfigException);
   
   private:

      // configurables

      std::string connInterfacesFile; // implicitly generates connInterfacesList
      std::string connInterfacesList; // comma-separated list

      bool        debugRunComponentThreads;
      bool        debugRunStartupTests;
      bool        debugRunComponentTests;
      bool        debugRunIntegrationTests;

      std::string storeMgmtdDirectory;
      bool        storeAllowFirstRunInit;

      unsigned    tuneNumWorkers;
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

      bool        sysOverrideStoredRoot;
      bool        sysAllowNewServers;
      bool        sysAllowNewTargets;
      unsigned    sysTargetOfflineTimeoutSecs;

      bool        runDaemonized;

      std::string pidFile;

      bool                 quotaEnableEnforcement;
      std::string          quotaNofificationMethod;
      unsigned             quotaNotfificationIntervalMin;
      std::string          quotaAdminEmailAddress;
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

      virtual void loadDefaults(bool addDashes);
      virtual void applyConfigMap(bool enableException, bool addDashes)
         throw(InvalidConfigException);
      virtual void initImplicitVals() throw(InvalidConfigException);
      void initQuotaQueryTypeNum() throw(InvalidConfigException);
      void initQuotaQueryRange() throw(InvalidConfigException);
      void initDynamicPoolsLimits();
      std::string createDefaultCfgFilename();


      
   public:
      // getters & setters

      std::string getConnInterfacesList() const
      {
         return connInterfacesList;
      }

      bool getDebugRunComponentThreads() const
      {
         return debugRunComponentThreads;
      }

      bool getDebugRunStartupTests() const
      {
         return debugRunStartupTests;
      }

      bool getDebugRunComponentTests() const
      {
         return debugRunComponentTests;
      }

      bool getDebugRunIntegrationTests() const
      {
         return debugRunIntegrationTests;
      }
      
      std::string getStoreMgmtdDirectory() const
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

      unsigned getTuneClientAutoRemoveMins() const
      {
         return tuneClientAutoRemoveMins;
      }

      int64_t getTuneMetaSpaceLowLimit()
      {
         return tuneMetaSpaceLowLimit;
      }

      int64_t getTuneMetaSpaceEmergencyLimit()
      {
         return tuneMetaSpaceEmergencyLimit;
      }

      bool getTuneMetaDynamicPools()
      {
         return tuneMetaDynamicPools;
      }

      int64_t getTuneMetaSpaceNormalSpreadThreshold()
      {
         return tuneMetaSpaceNormalSpreadThreshold;
      }

      int64_t getTuneMetaSpaceLowSpreadThreshold()
      {
         return tuneMetaSpaceLowSpreadThreshold;
      }

      int64_t getTuneMetaSpaceLowDynamicLimit()
      {
         return tuneMetaSpaceLowDynamicLimit;
      }

      int64_t getTuneMetaSpaceEmergencyDynamicLimit()
      {
         return tuneMetaSpaceEmergencyDynamicLimit;
      }

      int64_t getTuneMetaInodesLowLimit()
      {
         return tuneMetaInodesLowLimit;
      }

      int64_t getTuneMetaInodesEmergencyLimit()
      {
         return tuneMetaInodesEmergencyLimit;
      }

      int64_t getTuneMetaInodesNormalSpreadThreshold()
      {
         return tuneMetaInodesNormalSpreadThreshold;
      }

      int64_t getTuneMetaInodesLowSpreadThreshold()
      {
         return tuneMetaInodesLowSpreadThreshold;
      }

      int64_t getTuneMetaInodesLowDynamicLimit()
      {
         return tuneMetaInodesLowDynamicLimit;
      }

      int64_t getTuneMetaInodesEmergencyDynamicLimit()
      {
         return tuneMetaInodesEmergencyDynamicLimit;
      }

      int64_t getTuneStorageSpaceLowLimit()
      {
         return tuneStorageSpaceLowLimit;
      }

      int64_t getTuneStorageSpaceEmergencyLimit()
      {
         return tuneStorageSpaceEmergencyLimit;
      }

      bool getTuneStorageDynamicPools()
      {
         return tuneStorageDynamicPools;
      }

      int64_t getTuneStorageSpaceNormalSpreadThreshold()
      {
         return tuneStorageSpaceNormalSpreadThreshold;
      }

      int64_t getTuneStorageSpaceLowSpreadThreshold()
      {
         return tuneStorageSpaceLowSpreadThreshold;
      }

      int64_t getTuneStorageSpaceLowDynamicLimit()
      {
         return tuneStorageSpaceLowDynamicLimit;
      }

      int64_t getTuneStorageSpaceEmergencyDynamicLimit()
      {
         return tuneStorageSpaceEmergencyDynamicLimit;
      }

      int64_t getTuneStorageInodesLowLimit()
      {
         return tuneStorageInodesLowLimit;
      }

      int64_t getTuneStorageInodesEmergencyLimit()
      {
         return tuneStorageInodesEmergencyLimit;
      }

      int64_t getTuneStorageInodesNormalSpreadThreshold()
      {
         return tuneStorageInodesNormalSpreadThreshold;
      }

      int64_t getTuneStorageInodesLowSpreadThreshold()
      {
         return tuneStorageInodesLowSpreadThreshold;
      }

      int64_t getTuneStorageInodesLowDynamicLimit()
      {
         return tuneStorageInodesLowDynamicLimit;
      }

      int64_t getTuneStorageInodesEmergencyDynamicLimit()
      {
         return tuneStorageInodesEmergencyDynamicLimit;
      }

      unsigned getTuneProcessFDLimit() const
      {
         return tuneProcessFDLimit;
      }

      bool getSysOverrideStoredRoot() const
      {
         return sysOverrideStoredRoot;
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

      std::string getPIDFile() const
      {
         return pidFile;
      }

      bool getQuotaEnableEnforcement() const
      {
         return quotaEnableEnforcement;
      }

      std::string getQuotaNofificationMethod() const
      {
         return quotaNofificationMethod;
      }

      unsigned getQuotaNotfificationIntervalMin() const
      {
         return quotaNotfificationIntervalMin;
      }

      std::string getQuotaAdminEmailAddress() const
      {
         return quotaAdminEmailAddress;
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

      std::string getQuotaQueryGIDFile() const
      {
         return quotaQueryGIDFile;
      }

      std::string getQuotaQueryUIDFile() const
      {
         return quotaQueryUIDFile;
      }
};

#endif /*CONFIG_H_*/
