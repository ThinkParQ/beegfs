#include <common/toolkit/StringTk.h>
#include "Config.h"

#define CONFIG_DEFAULT_CFGFILENAME "/etc/beegfs/beegfs-helperd.conf"


Config::Config(int argc, char** argv) throw(InvalidConfigException) : AbstractConfig(argc, argv)
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
   AbstractConfig::loadDefaults();
   
   // re-definitions
   configMapRedefine("cfgFile",                  "" /*createDefaultCfgFilename()*/ );
   configMapRedefine("connUseRDMA",              "false"); // overrides abstract config
   
   configMapRedefine("logLevel",                 "2");

   // own definitions
   configMapRedefine("connInterfacesFile",       "");
   
   configMapRedefine("debugRunComponentThreads", "true");
   configMapRedefine("debugRunStartupTests",     "false");
   configMapRedefine("debugRunComponentTests",   "false");
   configMapRedefine("debugRunIntegrationTests", "false");
   
   configMapRedefine("tuneNumWorkers",           "2");

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
      if(iter->first == std::string("tuneNumWorkers") )
         tuneNumWorkers = StringTk::strToUInt(iter->second);
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

