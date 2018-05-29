#include <common/cache/toolkit/cachepathtk.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/UnitTk.h>
#include "Config.h"


#define CONFIG_DEFAULT_CFGFILENAME              "/etc/beegfs/beegfs-deeper-lib.conf"



/**
 * constructor
 */
Config::Config(int argc, char** argv): CacheConfig(argc, argv)
{  // do not throw exceptions, it is created from C code, exceptions will not work
   initConfig(argc, argv, false);
}

/**
 * Sets the default values for each configurable in the configMap.
 *
 * @param addDashes currently unused
 */
void Config::loadDefaults(bool addDashes)
{
   CacheConfig::loadDefaults();

   // re-definitions, override default, because the lib should log to the console and only errors
   configMapRedefine("cfgFile",                       createDefaultCfgFilename() );
   configMapRedefine("logLevel",                      "0");
   configMapRedefine("logErrsToStdlog",               "false");
   configMapRedefine("logStdFile",                    "");
   configMapRedefine("logErrFile",                    "");

   // own definitions
   configMapRedefine("sysCacheID",                    "");

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

      if(iter->first == std::string("sysCacheID") )
         sysCacheID = StringTk::strToUInt64(iter->second.c_str() );
      else if(iter->first == std::string("logLevelLib") )
         logLevel = StringTk::strToUInt64(iter->second.c_str() );
      else // ignore all values which are only for the cache daemon
         IGNORE_CONFIG_VALUE("logLevel")
         IGNORE_CONFIG_VALUE("logNoDate")
         IGNORE_CONFIG_VALUE("logNumLines")
         IGNORE_CONFIG_VALUE("logNumRotatedFiles")
         IGNORE_CONFIG_VALUE("logStdFile")
         IGNORE_CONFIG_VALUE("logErrFile")
         IGNORE_CONFIG_VALUE("runDaemonized")
         IGNORE_CONFIG_VALUE("tuneFileSplitSize")
         IGNORE_CONFIG_VALUE("tuneNumWorkers")
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

/*
 * creates the default path name of hte configuration file
 */
std::string Config::createDefaultCfgFilename() const
{
   struct stat statBuf;
   
   const int statRes = stat(CONFIG_DEFAULT_CFGFILENAME, &statBuf);
   
   if(!statRes && S_ISREG(statBuf.st_mode) )
      return CONFIG_DEFAULT_CFGFILENAME; // there appears to be a config file
   
   return ""; // no default file otherwise
}

void Config::initImplicitVals() 
{
   CacheConfig::initImplicitVals();

   cfgFile = createDefaultCfgFilename();

   //override log files, because only errors should be logged and only to the console
   logStdFile = "";
   logErrFile = "";
}
