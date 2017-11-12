#include <common/toolkit/StringTk.h>
#include <common/toolkit/UnitTk.h>
#include "Config.h"



Config::Config(int argc, char** argv) : 
      CacheConfig(argc, argv)
{
   initConfig(argc, argv, true);
}

/**
 * Sets the default values for each configurable in the configMap.
 *
 * @param addDashes currently unsused
 */
void Config::loadDefaults(bool addDashes)
{
   CacheConfig::loadDefaults();

   // re-definitions
   configMapRedefine("cfgFile",                       createDefaultCfgFilename() );

   configMapRedefine("logErrsToStdlog",               "true");

   configMapRedefine("pidFile",                       "");

   configMapRedefine("runDaemonized",                 "false");

   // own definitions
   configMapRedefine("tuneFileSplitSize",             "500m");
   configMapRedefine("tuneNumStreamListeners",        "10");
   configMapRedefine("tuneNumWorkers",                "0");
   configMapRedefine("tuneWorkerNumaAffinity",        "false");
   configMapRedefine("tuneListenerNumaAffinity",      "false");
   configMapRedefine("tuneBindToNumaZone",            "");
   configMapRedefine("tuneWorkerBufSize",             "1m");
   configMapRedefine("tuneUseAggressiveStreamPoll",   "false");
   configMapRedefine("logHealthCheckIntervalSec",     "0");
}

/**
 * @param addDashes currently usused
 */
void Config::applyConfigMap(bool enableException, bool addDashes)
{
   CacheConfig::applyConfigMap(false);

   for(StringMapIter iter = configMap.begin(); iter != configMap.end(); )
   {
      bool unknownElement = false;

      if(iter->first == std::string("pidFile") )
         pidFile = iter->second;
      else if(iter->first == std::string("runDaemonized") )
         runDaemonized = StringTk::strToBool(iter->second);
      else if(iter->first == std::string("tuneFileSplitSize") )
         tuneFileSplitSize = UnitTk::strHumanToInt64(iter->second);
      else if(iter->first == std::string("tuneNumStreamListeners") )
         tuneNumStreamListeners = StringTk::strToUInt(iter->second);
      else if(iter->first == std::string("tuneNumWorkers") )
         tuneNumWorkers = StringTk::strToUInt(iter->second);
      else if(iter->first == std::string("tuneWorkerBufSize") )
         tuneWorkerBufSize = UnitTk::strHumanToInt64(iter->second);
      else if(iter->first == std::string("tuneUseAggressiveStreamPoll") )
         tuneUseAggressiveStreamPoll = StringTk::strToBool(iter->second);
      else if(iter->first == std::string("tuneWorkerNumaAffinity") )
         tuneWorkerNumaAffinity = StringTk::strToBool(iter->second);
      else if(iter->first == std::string("tuneListenerNumaAffinity") )
         tuneListenerNumaAffinity = StringTk::strToBool(iter->second);
      else if(iter->first == std::string("tuneBindToNumaZone") )
      {
         if(iter->second.empty() ) // not defined => disable
            tuneBindToNumaZone = -1; // -1 means disable binding
         else
            tuneBindToNumaZone = StringTk::strToInt(iter->second);
      }
      else if(iter->first == std::string("logHealthCheckIntervalSec") )
         logHealthCheckIntervalSec = StringTk::strToUInt64(iter->second.c_str() );
      else // ignore all values which are only for the cache lib
         IGNORE_CONFIG_VALUE("sysCacheID")
         IGNORE_CONFIG_VALUE("logLevelLib")
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

void Config::initImplicitVals()
{
   cfgFile = createDefaultCfgFilename();

   // connAuthHash
   CacheConfig::initConnAuthHash(connAuthFile, &connAuthHash);
}

std::string Config::createDefaultCfgFilename() const
{
   struct stat statBuf;

   const int statRes = stat(CONFIG_DEFAULT_CFGFILENAME, &statBuf);

   if(!statRes && S_ISREG(statBuf.st_mode) )
      return CONFIG_DEFAULT_CFGFILENAME; // there appears to be a config file

   return ""; // no default file otherwise
}

