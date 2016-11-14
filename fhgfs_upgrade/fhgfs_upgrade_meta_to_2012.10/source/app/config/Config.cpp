#include <common/toolkit/StringTk.h>
#include <common/toolkit/UnitTk.h>
#include <common/system/System.h>
#include "Config.h"
#include <upgrade/Upgrade.h>

//#define CONFIG_DEFAULT_CFGFILENAME "/etc/fhgfs/fhgfs-meta.conf"
#define CONFIG_UPGRADE_LOGFILENAME   "/var/tmp/fhgfs-upgrade-meta-"
#define CONFIG_LOG_EXTENSION         ".log"


#define TARGETCHOOSERTYPE_RANDOMIZED_STR   "randomized"
#define TARGETCHOOSERTYPE_ROUNDROBIN_STR   "roundrobin"
#define TARGETCHOOSERTYPE_RANDOMROBIN_STR  "randomrobin"


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

   this->logStdFile = std::string(CONFIG_UPGRADE_LOGFILENAME) +
      StringTk::uint64ToStr(System::getPID() ) + std::string(CONFIG_LOG_EXTENSION);

   this->logLevel = Log_NOTICE;

   this->deleteOldFiles = false; // create backup files by default


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

      if(iter->first == std::string("storeMetaDirectory") ) // stringID to numID mapping
         storeMetaDirectory = iter->second;
      else
      if(iter->first == std::string("targetIdMapFile") ) // stringID to numID mapping
         targetIdMapFile = iter->second;
      else
      if(iter->first == std::string("metaIdMapFile") ) // stringID to numID mapping
         metaIdMapFile = iter->second;
      else
      if(iter->first == std::string("upgradeDeleteOldFiles") ) // delete migrated old files
         deleteOldFiles = StringTk::strToBool(iter->second);
      else
      if (iter->first == std::string("--help") || iter->first == std::string("-h") )
      {
         char* progPath = this->argv[0];
         std::string progName = basename(progPath);

         Upgrade::printUsage(progName);;
         ::exit(1);
      }
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
}

void Config::initTuneTargetChooserNum() throw(InvalidConfigException)
{
}

std::string Config::createDefaultCfgFilename()
{
#if 0
   struct stat statBuf;

   int statRes = stat(CONFIG_DEFAULT_CFGFILENAME, &statBuf);

   if(!statRes && S_ISREG(statBuf.st_mode) )
      return CONFIG_DEFAULT_CFGFILENAME; // there appears to be a config file

#endif

   return ""; // no default file otherwise
}

