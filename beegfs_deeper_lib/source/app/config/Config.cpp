#include <common/toolkit/StringTk.h>
#include <common/toolkit/UnitTk.h>
#include <toolkit/CachePathTk.h>
#include "Config.h"


#define CONFIG_DEFAULT_CFGFILENAME              "/etc/beegfs/beegfs-deeper-lib.conf"



/**
 * constructor
 */
Config::Config(int argc, char** argv) throw(InvalidConfigException) : AbstractConfig(argc, argv)
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
   AbstractConfig::loadDefaults();

   // re-definitions
   configMapRedefine("cfgFile",                       createDefaultCfgFilename() );
   configMapRedefine("logLevel",                      "0");
   configMapRedefine("logErrsToStdlog",               "false");
   configMapRedefine("logStdFile",                    "");
   configMapRedefine("logErrFile",                    "");

   // own definitions
   configMapRedefine("sysMountPointCache",            "");
   configMapRedefine("sysMountPointGlobal",           "");

   configMapRedefine("sysCacheID",                    "");

   configMapRedefine("connDeeperCachedNamedSocket",   "/var/run/beegfs-deeper-cached.sock");
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
      
      if(iter->first == std::string("sysMountPointCache") )
         sysMountPointCache = iter->second;
      else
      if(iter->first == std::string("sysMountPointGlobal") )
         sysMountPointGlobal = iter->second;
      else
      if(iter->first == std::string("sysCacheID") )
         sysCacheID = StringTk::strToUInt64(iter->second.c_str() );
      else
      if(iter->first == std::string("connDeeperCachedNamedSocket") )
         connDeeperCachedNamedSocket = iter->second;
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

/*
 * creates the default path name of hte configuration file
 */
std::string Config::createDefaultCfgFilename()
{
   struct stat statBuf;
   
   int statRes = stat(CONFIG_DEFAULT_CFGFILENAME, &statBuf);
   
   if(!statRes && S_ISREG(statBuf.st_mode) )
      return CONFIG_DEFAULT_CFGFILENAME; // there appears to be a config file
   
   return ""; // no default file otherwise
}

void Config::initImplicitVals() throw(InvalidConfigException)
{
   cfgFile = createDefaultCfgFilename();
   CachePathTk::preparePaths(this->sysMountPointCache);
   CachePathTk::preparePaths(this->sysMountPointGlobal);
}
