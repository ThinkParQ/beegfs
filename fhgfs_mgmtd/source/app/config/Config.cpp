#include <common/toolkit/StringTk.h>
#include <common/toolkit/UnitTk.h>
#include "Config.h"

#define CONFIG_DEFAULT_CFGFILENAME              "/etc/beegfs/beegfs-mgmtd.conf"
#define QUOTA_EXCEEDED_RANGE_CONFIG_DELIMITER   ','


Config::Config(int argc, char** argv) throw(InvalidConfigException) : AbstractConfig(argc, argv)
{
   initConfig(argc, argv, true);
}

/**
 * Sets the default values for each configurable in the configMap.
 *
 * @param addDashes currently unused
 */
void Config::loadDefaults(bool addDashes)
{
   AbstractConfig::loadDefaults();

   // re-definitions
   configMapRedefine("cfgFile",                         "");
   configMapRedefine("connUseRDMA",                     "false"); // overrides abstract config

   // own definitions
   configMapRedefine("connInterfacesFile",              "");
   configMapRedefine("connInterfacesList",              "");

   configMapRedefine("debugRunComponentThreads",        "true");
   configMapRedefine("debugRunStartupTests",            "false");
   configMapRedefine("debugRunComponentTests",          "false");
   configMapRedefine("debugRunIntegrationTests",        "false");

   configMapRedefine("storeMgmtdDirectory",             "");
   configMapRedefine("storeAllowFirstRunInit",          "true");

   configMapRedefine("tuneNumWorkers",                        "4");
   configMapRedefine("tuneNumQuotaWorkers",                   "8");
   configMapRedefine("tuneMetaNodeAutoRemoveMins",            "0"); // ignored, only for compat
   configMapRedefine("tuneStorageNodeAutoRemoveMins",         "0"); // ignored, only for compat
   configMapRedefine("tuneClientAutoRemoveMins",              "60");

   configMapRedefine("tuneMetaSpaceLowLimit",                 "10G");
   configMapRedefine("tuneMetaSpaceEmergencyLimit",           "3G");
   configMapRedefine("tuneMetaInodesLowLimit",                "10M");
   configMapRedefine("tuneMetaInodesEmergencyLimit",          "1M");
   configMapRedefine("tuneMetaDynamicPools",                  "true");

   configMapRedefine("tuneMetaSpaceNormalSpreadThreshold",    "0");
   configMapRedefine("tuneMetaSpaceLowSpreadThreshold",       "0");
   configMapRedefine("tuneMetaSpaceLowDynamicLimit",          "0");
   configMapRedefine("tuneMetaSpaceEmergencyDynamicLimit",    "0");
   configMapRedefine("tuneMetaInodesNormalSpreadThreshold",   "0");
   configMapRedefine("tuneMetaInodesLowSpreadThreshold",      "0");
   configMapRedefine("tuneMetaInodesLowDynamicLimit",         "0");
   configMapRedefine("tuneMetaInodesEmergencyDynamicLimit",   "0");

   configMapRedefine("tuneStorageSpaceLowLimit",              "1T");
   configMapRedefine("tuneStorageSpaceEmergencyLimit",        "20G");
   configMapRedefine("tuneStorageInodesLowLimit",             "10M");
   configMapRedefine("tuneStorageInodesEmergencyLimit",       "1M");
   configMapRedefine("tuneStorageDynamicPools",               "true");

   configMapRedefine("tuneStorageSpaceNormalSpreadThreshold", "0");
   configMapRedefine("tuneStorageSpaceLowSpreadThreshold",    "0");
   configMapRedefine("tuneStorageSpaceLowDynamicLimit",       "0");
   configMapRedefine("tuneStorageSpaceEmergencyDynamicLimit", "0");
   configMapRedefine("tuneStorageInodesNormalSpreadThreshold","0");
   configMapRedefine("tuneStorageInodesLowSpreadThreshold",   "0");
   configMapRedefine("tuneStorageInodesLowDynamicLimit",      "0");
   configMapRedefine("tuneStorageInodesEmergencyDynamicLimit","0");

   configMapRedefine("tuneProcessFDLimit",                    "10000");

   configMapRedefine("sysOverrideStoredRoot",           "false");
   configMapRedefine("sysAllowNewServers",              "true");
   configMapRedefine("sysAllowNewTargets",              "true");
   configMapRedefine("sysTargetOfflineTimeoutSecs",     "180");

   configMapRedefine("runDaemonized",                   "false");

   configMapRedefine("pidFile",                         "");

   configMapRedefine("quotaEnableEnforcement",          "false");
   configMapRedefine("quotaNofificationMethod",         "disabled");
   configMapRedefine("quotaNotfificationIntervalMin",   "360");
   configMapRedefine("quotaAdminEmailAddress",          "");
   configMapRedefine("quotaUpdateIntervalMin",          "10");
   configMapRedefine("quotaStoreIntervalMin",           "10");
   configMapRedefine("quotaQueryType",                  MGMT_QUOTA_QUERY_TYPE_SYSTEM_STR);
   configMapRedefine("quotaQueryUIDFile",               "");
   configMapRedefine("quotaQueryGIDFile",               "");
   configMapRedefine("quotaQueryUIDRange",              "");
   configMapRedefine("quotaQueryGIDRange",              "");
   configMapRedefine("quotaQueryWithSystemUsersGroups", "false");
}

/**
 * @param addDashes currently usused
 */
void Config::applyConfigMap(bool enableException, bool addDashes) throw(InvalidConfigException)
{
   AbstractConfig::applyConfigMap(false);
   
   for(StringMapIter iter = getConfigMap()->begin(); iter != getConfigMap()->end(); )
   {
      bool unknownElement = false;
      
      if(iter->first == std::string("connInterfacesFile") )
         connInterfacesFile = iter->second;
      else
      if(iter->first == std::string("connInterfacesList") )
         connInterfacesList = iter->second;
      else
      if(iter->first == std::string("debugRunComponentThreads") )
         debugRunComponentThreads = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("debugRunStartupTests") )
         debugRunStartupTests = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("debugRunComponentTests") )
         debugRunComponentTests = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("debugRunIntegrationTests") )
         debugRunIntegrationTests = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("storeMgmtdDirectory") )
         storeMgmtdDirectory = iter->second;
      else
      if(iter->first == std::string("storeAllowFirstRunInit") )
         storeAllowFirstRunInit = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("tuneNumWorkers") )
         tuneNumWorkers = StringTk::strToUInt(iter->second);
      else
      if(iter->first == std::string("tuneNumQuotaWorkers") )
         tuneNumQuotaWorkers = StringTk::strToUInt(iter->second);
      else
      if(iter->first == std::string("tuneMetaNodeAutoRemoveMins") )
         tuneMetaNodeAutoRemoveMins = StringTk::strToUInt(iter->second);
      else
      if(iter->first == std::string("tuneStorageNodeAutoRemoveMins") )
         tuneStorageNodeAutoRemoveMins = StringTk::strToUInt(iter->second);
      else
      if(iter->first == std::string("tuneClientAutoRemoveMins") )
         tuneClientAutoRemoveMins = StringTk::strToUInt(iter->second);
      else
      if(iter->first == std::string("tuneMetaSpaceLowLimit") )
         tuneMetaSpaceLowLimit = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneMetaSpaceEmergencyLimit") )
         tuneMetaSpaceEmergencyLimit = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneMetaDynamicPools") )
         tuneMetaDynamicPools = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("tuneMetaSpaceNormalSpreadThreshold") )
         tuneMetaSpaceNormalSpreadThreshold = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneMetaSpaceLowSpreadThreshold") )
         tuneMetaSpaceLowSpreadThreshold = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneMetaSpaceLowDynamicLimit") )
         tuneMetaSpaceLowDynamicLimit = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneMetaSpaceEmergencyDynamicLimit") )
         tuneMetaSpaceEmergencyDynamicLimit = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneMetaInodesLowLimit") )
         tuneMetaInodesLowLimit = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneMetaInodesEmergencyLimit") )
         tuneMetaInodesEmergencyLimit = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneMetaInodesNormalSpreadThreshold") )
         tuneMetaInodesNormalSpreadThreshold = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneMetaInodesLowSpreadThreshold") )
         tuneMetaInodesLowSpreadThreshold = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneMetaInodesLowDynamicLimit") )
         tuneMetaInodesLowDynamicLimit = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneMetaInodesEmergencyDynamicLimit") )
         tuneMetaInodesEmergencyDynamicLimit = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneStorageSpaceLowLimit") )
         tuneStorageSpaceLowLimit = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneStorageSpaceEmergencyLimit") )
         tuneStorageSpaceEmergencyLimit = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneStorageDynamicPools") )
         tuneStorageDynamicPools = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("tuneStorageSpaceNormalSpreadThreshold") )
         tuneStorageSpaceNormalSpreadThreshold = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneStorageSpaceLowSpreadThreshold") )
         tuneStorageSpaceLowSpreadThreshold = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneStorageSpaceLowDynamicLimit") )
         tuneStorageSpaceLowDynamicLimit = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneStorageSpaceEmergencyDynamicLimit") )
         tuneStorageSpaceEmergencyDynamicLimit = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneStorageInodesLowLimit") )
         tuneStorageInodesLowLimit = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneStorageInodesEmergencyLimit") )
         tuneStorageInodesEmergencyLimit = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneStorageInodesNormalSpreadThreshold") )
         tuneStorageInodesNormalSpreadThreshold = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneStorageInodesLowSpreadThreshold") )
         tuneStorageInodesLowSpreadThreshold = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneStorageInodesLowDynamicLimit") )
         tuneStorageInodesLowDynamicLimit = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneStorageInodesEmergencyDynamicLimit") )
         tuneStorageInodesEmergencyDynamicLimit = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneProcessFDLimit") )
         tuneProcessFDLimit = StringTk::strToUInt(iter->second);
      else
      if(iter->first == std::string("sysOverrideStoredRoot") )
         sysOverrideStoredRoot = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("sysAllowNewServers") )
         sysAllowNewServers = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("sysAllowNewTargets") )
         sysAllowNewTargets = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("sysTargetOfflineTimeoutSecs") )
      {
         sysTargetOfflineTimeoutSecs = StringTk::strToUInt(iter->second);
         if (sysTargetOfflineTimeoutSecs < 30)
            throw InvalidConfigException("Invalid sysTargetOfflineTimeoutSecs value "
                  + iter->second + " (must be at least 30)");
      }
      else
      if(iter->first == std::string("runDaemonized") )
         runDaemonized = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("pidFile") )
         pidFile = iter->second;
      else
      if(iter->first == std::string("quotaEnableEnforcement") )
         quotaEnableEnforcement = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("quotaNofificationMethod") )
         quotaNofificationMethod = iter->second;
      else
      if(iter->first == std::string("quotaNotfificationIntervalMin") )
         quotaNotfificationIntervalMin = StringTk::strToUInt(iter->second);
      else
      if(iter->first == std::string("quotaAdminEmailAddress") )
         quotaAdminEmailAddress = iter->second;
      else
      if(iter->first == std::string("quotaUpdateIntervalMin") )
         quotaUpdateIntervalMin = StringTk::strToUInt(iter->second);
      else
      if(iter->first == std::string("quotaStoreIntervalMin") )
         quotaStoreIntervalMin = StringTk::strToUInt(iter->second);
      else
      if(iter->first == std::string("quotaQueryType") )
         quotaQueryType = iter->second;
      else
      if(iter->first == std::string("quotaQueryUIDFile") )
         quotaQueryUIDFile = iter->second;
      else
      if(iter->first == std::string("quotaQueryGIDFile") )
         quotaQueryGIDFile = iter->second;
      else
      if(iter->first == std::string("quotaQueryUIDRange") )
         quotaQueryUIDRange = iter->second;
      else
      if(iter->first == std::string("quotaQueryGIDRange") )
         quotaQueryGIDRange = iter->second;
      else
      if(iter->first == std::string("quotaQueryWithSystemUsersGroups") )
         quotaQueryWithSystemUsersGroups = StringTk::strToBool(iter->second);
      else
      { // unknown element occurred
         unknownElement = true;
         
         if(enableException)
         {
            throw InvalidConfigException(
               std::string("The config argument '") + iter->first + std::string("' is invalid") );
         }
      }
      
      if(unknownElement)
      { // just skip the unknown element
         iter++;
      }
      else
      { // remove this element from the map
         iter = eraseFromConfigMap(iter);
      }
   }
}

void Config::initImplicitVals() throw(InvalidConfigException)
{
   // connInterfacesList(/File)
   AbstractConfig::initInterfacesList(connInterfacesFile, connInterfacesList);

   // connAuthHash
   AbstractConfig::initConnAuthHash(connAuthFile, &connAuthHash);

   // quotaQueryTypeNum
   initQuotaQueryTypeNum();

   // quotaQueryUIDRangeStart, quotaQueryUIDRangeEnd, quotaQueryGIDRangeStart, quotaQueryGIDRangeEnd
   if(quotaQueryTypeNum == MgmtQuotaQueryType_RANGE)
      initQuotaQueryRange();

   initDynamicPoolsLimits();
}

std::string Config::createDefaultCfgFilename()
{
   struct stat statBuf;
   
   int statRes = stat(CONFIG_DEFAULT_CFGFILENAME, &statBuf);
   
   if(!statRes && S_ISREG(statBuf.st_mode) )
      return CONFIG_DEFAULT_CFGFILENAME; // there appears to be a config file
   
   return ""; // no default file otherwise
}

void Config::initQuotaQueryTypeNum() throw(InvalidConfigException)
{
   if(this->quotaQueryType == MGMT_QUOTA_QUERY_TYPE_SYSTEM_STR)
      this->quotaQueryTypeNum = MgmtQuotaQueryType_SYSTEM;
   else if(this->quotaQueryType == MGMT_QUOTA_QUERY_TYPE_RANGE_STR)
      this->quotaQueryTypeNum = MgmtQuotaQueryType_RANGE;
   else if(this->quotaQueryType == MGMT_QUOTA_QUERY_TYPE_FILE_STR)
      this->quotaQueryTypeNum = MgmtQuotaQueryType_FILE;
   else
      throw InvalidConfigException("Invalid quota query type specified: " + quotaQueryType);
}

void Config::initQuotaQueryRange() throw(InvalidConfigException)
{
   if(this->quotaQueryUIDRange.empty() )
      throw InvalidConfigException("No UID range defined");

   StringList uidRange;
   StringTk::explodeEx(this->quotaQueryUIDRange, QUOTA_EXCEEDED_RANGE_CONFIG_DELIMITER, true,
      &uidRange);

   if(uidRange.size() == 2)
   {
      if(StringTk::isNumeric(uidRange.front() ) && StringTk::isNumeric(uidRange.back() ) )
      {
         this->quotaQueryUIDRangeStart = StringTk::strToUInt(uidRange.front());
         this->quotaQueryUIDRangeEnd = StringTk::strToUInt(uidRange.back());
      }
      else
         throw InvalidConfigException("Invalid UID range defined. Only numeric values allowed.");

      if(this->quotaQueryUIDRangeStart > this->quotaQueryUIDRangeEnd)
      {
         unsigned tmp = this->quotaQueryUIDRangeStart;
         this->quotaQueryUIDRangeStart = this->quotaQueryUIDRangeEnd;
         this->quotaQueryUIDRangeEnd = tmp;
      }
   }
   else
      throw InvalidConfigException("Invalid UID range defined");


   if(this->quotaQueryGIDRange.empty() )
      throw InvalidConfigException("No GID range defined");

   StringList gidRange;
   StringTk::explodeEx(this->quotaQueryGIDRange, QUOTA_EXCEEDED_RANGE_CONFIG_DELIMITER, true,
      &gidRange);

   if(gidRange.size() == 2)
   {
      if(StringTk::isNumeric(gidRange.front() ) && StringTk::isNumeric(gidRange.back() ) )
      {
         this->quotaQueryGIDRangeStart = StringTk::strToUInt(gidRange.front());
         this->quotaQueryGIDRangeEnd = StringTk::strToUInt(gidRange.back());
      }
      else
         throw InvalidConfigException("Invalid GID range defined. Only numeric values allowed.");

      if(this->quotaQueryGIDRangeStart > this->quotaQueryGIDRangeEnd)
      {
         unsigned tmp = this->quotaQueryGIDRangeStart;
         this->quotaQueryGIDRangeStart = this->quotaQueryGIDRangeEnd;
         this->quotaQueryGIDRangeEnd = tmp;
      }
   }
   else
      throw InvalidConfigException("Invalid GID range defined");
}

void Config::initDynamicPoolsLimits()
{
   // meta space values...

   if(!tuneMetaSpaceNormalSpreadThreshold)
      tuneMetaSpaceNormalSpreadThreshold = tuneMetaSpaceLowLimit;

   if(!tuneMetaSpaceLowSpreadThreshold)
      tuneMetaSpaceLowSpreadThreshold = tuneMetaSpaceEmergencyLimit;

   if(!tuneMetaSpaceLowDynamicLimit)
      tuneMetaSpaceLowDynamicLimit = tuneMetaSpaceLowLimit*2;

   if(!tuneMetaSpaceEmergencyDynamicLimit)
      tuneMetaSpaceEmergencyDynamicLimit = tuneMetaSpaceEmergencyDynamicLimit*2;

   // meta inodes values...

   if(!tuneMetaInodesNormalSpreadThreshold)
      tuneMetaInodesNormalSpreadThreshold = tuneMetaInodesLowLimit;

   if(!tuneMetaInodesLowSpreadThreshold)
      tuneMetaInodesLowSpreadThreshold = tuneMetaInodesEmergencyLimit;

   if(!tuneMetaInodesLowDynamicLimit)
      tuneMetaInodesLowDynamicLimit = tuneMetaInodesLowLimit*2;

   if(!tuneMetaInodesEmergencyDynamicLimit)
      tuneMetaInodesEmergencyDynamicLimit = tuneMetaInodesEmergencyDynamicLimit*2;


   // storage space values...

   if(!tuneStorageSpaceNormalSpreadThreshold)
      tuneStorageSpaceNormalSpreadThreshold = tuneStorageSpaceLowLimit;

   if(!tuneStorageSpaceLowSpreadThreshold)
      tuneStorageSpaceLowSpreadThreshold = tuneStorageSpaceEmergencyLimit;

   if(!tuneStorageSpaceLowDynamicLimit)
      tuneStorageSpaceLowDynamicLimit = tuneStorageSpaceLowLimit*2;

   if(!tuneStorageSpaceEmergencyDynamicLimit)
      tuneStorageSpaceEmergencyDynamicLimit = tuneStorageSpaceEmergencyDynamicLimit*2;

   // storage inodes values...

   if(!tuneStorageInodesNormalSpreadThreshold)
      tuneStorageInodesNormalSpreadThreshold = tuneStorageInodesLowLimit;

   if(!tuneStorageInodesLowSpreadThreshold)
      tuneStorageInodesLowSpreadThreshold = tuneStorageInodesEmergencyLimit;

   if(!tuneStorageInodesLowDynamicLimit)
      tuneStorageInodesLowDynamicLimit = tuneStorageInodesLowLimit*2;

   if(!tuneStorageInodesEmergencyDynamicLimit)
      tuneStorageInodesEmergencyDynamicLimit = tuneStorageInodesEmergencyDynamicLimit*2;

}
