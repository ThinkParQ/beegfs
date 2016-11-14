#include <common/toolkit/StringTk.h>
#include <common/toolkit/UnitTk.h>
#include "Config.h"

#define CONFIG_DEFAULT_CFGFILENAME "/etc/beegfs/beegfs-storage.conf"


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
   configMapRedefine("cfgFile",                  "");

   // own definitions
   configMapRedefine("connInterfacesFile",       "");
   configMapRedefine("connInterfacesList",       "");

   configMapRedefine("debugRunComponentThreads", "true");
   configMapRedefine("debugRunStartupTests",     "false");
   configMapRedefine("debugRunComponentTests",   "false");
   configMapRedefine("debugRunIntegrationTests", "false");

   configMapRedefine("storeStorageDirectory",    "");
   configMapRedefine("storeAllowFirstRunInit",   "true");
   configMapRedefine("storeInitHashDirs",        "false");

   configMapRedefine("tuneNumStreamListeners",        "1");
   configMapRedefine("tuneNumWorkers",                "8");
   configMapRedefine("tuneWorkerBufSize",             "4m");
   configMapRedefine("tuneProcessFDLimit",            "50000");
   configMapRedefine("tuneWorkerNumaAffinity",        "false");
   configMapRedefine("tuneListenerNumaAffinity",      "false");
   configMapRedefine("tuneListenerPrioShift",         "-1");
   configMapRedefine("tuneBindToNumaZone",            "");
   configMapRedefine("tuneFileReadSize",              "32k");
   configMapRedefine("tuneFileReadAheadTriggerSize",  "4m");
   configMapRedefine("tuneFileReadAheadSize",         "0");
   configMapRedefine("tuneFileWriteSize",             "64k");
   configMapRedefine("tuneFileWriteSyncSize",         "0");
   configMapRedefine("tuneUsePerUserMsgQueues",       "false");
   configMapRedefine("tuneDirCacheLimit",             "1024");
   configMapRedefine("tuneEarlyStat",                 "false");
   configMapRedefine("tuneNumResyncSlaves",           "12");
   configMapRedefine("tuneNumResyncGatherSlaves",     "6");
   configMapRedefine("tuneUseAggressiveStreamPoll",   "false");
   configMapRedefine("tuneUsePerTargetWorkers",       "true");

   configMapRedefine("quotaEnableEnforcement",        "false");
   configMapRedefine("quotaDisableZfsSupport",        "false");

   configMapRedefine("sysResyncSafetyThresholdMins",  "10");
   configMapRedefine("sysTargetOfflineTimeoutSecs",   "180");

   configMapRedefine("runDaemonized",            "false");

   configMapRedefine("pidFile",                  "");
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
      if(iter->first == std::string("storeStorageDirectory") )
         storeStorageDirectory = iter->second;
      else
      if(iter->first == std::string("storeAllowFirstRunInit") )
         storeAllowFirstRunInit = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("storeInitHashDirs") )
         storeInitHashDirs = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("tuneNumStreamListeners") )
         tuneNumStreamListeners = StringTk::strToUInt(iter->second);
      else
      if(iter->first == std::string("tuneNumWorkers") )
         tuneNumWorkers = StringTk::strToUInt(iter->second);
      else
      if(iter->first == std::string("tuneWorkerBufSize") )
         tuneWorkerBufSize = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneProcessFDLimit") )
         tuneProcessFDLimit = StringTk::strToUInt(iter->second);
      else
      if(iter->first == std::string("tuneWorkerNumaAffinity") )
         tuneWorkerNumaAffinity = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("tuneListenerNumaAffinity") )
         tuneListenerNumaAffinity = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("tuneBindToNumaZone") )
      {
         if(iter->second.empty() ) // not defined => disable
            tuneBindToNumaZone = -1; // -1 means disable binding
         else
            tuneBindToNumaZone = StringTk::strToInt(iter->second);
      }
      else
      if(iter->first == std::string("tuneListenerPrioShift") )
         tuneListenerPrioShift = StringTk::strToInt(iter->second);
      else
      if(iter->first == std::string("tuneFileReadSize") )
         tuneFileReadSize = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneFileReadAheadTriggerSize") )
         tuneFileReadAheadTriggerSize = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneFileReadAheadSize") )
         tuneFileReadAheadSize = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneFileWriteSize") )
         tuneFileWriteSize = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneFileWriteSyncSize") )
         tuneFileWriteSyncSize = UnitTk::strHumanToInt64(iter->second);
      else
      if(iter->first == std::string("tuneUsePerUserMsgQueues") )
         tuneUsePerUserMsgQueues = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("tuneDirCacheLimit") )
         tuneDirCacheLimit = StringTk::strToUInt(iter->second);
      else
      if (iter->first == std::string("tuneEarlyStat") )
         this->tuneEarlyStat = StringTk::strToBool(iter->second);
      else
      if (iter->first == std::string("tuneNumResyncGatherSlaves") )
         this->tuneNumResyncGatherSlaves = StringTk::strToUInt(iter->second);
      else
      if (iter->first == std::string("tuneNumResyncSlaves") )
         this->tuneNumResyncSlaves = StringTk::strToUInt(iter->second);
      else
      if(iter->first == std::string("tuneUseAggressiveStreamPoll") )
         tuneUseAggressiveStreamPoll = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("tuneUsePerTargetWorkers") )
         tuneUsePerTargetWorkers = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("quotaEnableEnforcement") )
         quotaEnableEnforcement = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("quotaDisableZfsSupport") )
         quotaDisableZfsSupport = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("sysResyncSafetyThresholdMins") )
         sysResyncSafetyThresholdMins = StringTk::strToInt64(iter->second);
      else
      if(iter->first == std::string("sysTargetOfflineTimeoutSecs") )
         sysTargetOfflineTimeoutSecs = StringTk::strToUInt(iter->second);
      else
      if(iter->first == std::string("runDaemonized") )
         runDaemonized = StringTk::strToBool(iter->second);
      else
      if(iter->first == std::string("pidFile") )
         pidFile = iter->second;
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
   // tuneFileReadAheadTriggerSize (should be ">= tuneFileReadAheadSize")
   if(tuneFileReadAheadTriggerSize < tuneFileReadAheadSize)
      tuneFileReadAheadTriggerSize = tuneFileReadAheadSize;

   // connInterfacesList(/File)
   AbstractConfig::initInterfacesList(connInterfacesFile, connInterfacesList);

   // check if sync_file_range was enabled on a distro that doesn't support it
   #ifndef CONFIG_DISTRO_HAS_SYNC_FILE_RANGE
      if(tuneFileWriteSyncSize)
         throw InvalidConfigException(
            "Config option is not supported for this distribution: 'tuneFileWriteSyncSize'");
   #endif

   // connAuthHash
   AbstractConfig::initConnAuthHash(connAuthFile, &connAuthHash);
}

std::string Config::createDefaultCfgFilename()
{
   struct stat statBuf;

   int statRes = stat(CONFIG_DEFAULT_CFGFILENAME, &statBuf);

   if(!statRes && S_ISREG(statBuf.st_mode) )
      return CONFIG_DEFAULT_CFGFILENAME; // there appears to be a config file

   return ""; // no default file otherwise
}

