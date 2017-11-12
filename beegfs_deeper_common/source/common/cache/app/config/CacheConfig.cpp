#include <common/toolkit/StringTk.h>
#include <common/toolkit/UnitTk.h>
#include "CacheConfig.h"
#include "../../toolkit/cachepathtk.h"



/**
 * constructor
 */
CacheConfig::CacheConfig(int argc, char** argv): 
      AbstractConfig(argc, argv)
{  // do not throw exceptions, it is created from C code, exceptions will not work
   initConfig(argc, argv, false);
}

/**
 * Sets the default values for each configurable in the configMap.
 *
 * @param addDashes currently unused
 */
void CacheConfig::loadDefaults(bool addDashes)
{
   AbstractConfig::loadDefaults();

   // re-definitions
   configMapRedefine("cfgFile",                       CONFIG_DEFAULT_CFGFILENAME);
   configMapRedefine("logLevel",                      "0");
   configMapRedefine("logErrsToStdlog",               "false");
   configMapRedefine("logStdFile",                    "");
   configMapRedefine("logErrFile",                    "");

   // own definitions
   configMapRedefine("sysMountPointCache",            "");
   configMapRedefine("sysMountPointGlobal",           "");

   configMapRedefine("connNamedSocket",               CONFIG_DEFAULT_NAMEDSOCKETPATH);
}

/**
 * @param addDashes currently unused
 */
void CacheConfig::applyConfigMap(bool enableException, bool addDashes)
{
   AbstractConfig::applyConfigMap(false);

   for(StringMapIter iter = configMap.begin(); iter != configMap.end(); )
   {
      bool unknownElement = false;

      if(iter->first == std::string("sysMountPointCache") )
         sysMountPointCache = iter->second;
      else if(iter->first == std::string("sysMountPointGlobal") )
         sysMountPointGlobal = iter->second;
      else if(iter->first == std::string("connNamedSocket") )
         connNamedSocket = iter->second;
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

void CacheConfig::initImplicitVals()
{
   cachepathtk::preparePaths(this->sysMountPointCache);
   cachepathtk::preparePaths(this->sysMountPointGlobal);
}
