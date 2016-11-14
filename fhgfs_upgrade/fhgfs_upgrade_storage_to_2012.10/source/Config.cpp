#include"StringTk.h"
#include "Config.h"
#include "Logger.h"
#include "LogContext.h"
#include "System.h"

#define CONFIG_UPGRADE_LOGFILENAME   "/var/tmp/fhgfs-upgrade-storage-"
#define CONFIG_LOG_EXTENSION         ".log"


Config::Config(int argc, char** argv) : AbstractConfig(argc, argv)
{
   this->isValid = true;
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
   this->logErrsToStdlog = true;
   this->logLevel = Log_NOTICE;
}

/**
 * @param addDashes currently usused
 */
bool Config::applyConfigMap(bool enableException, bool addDashes)
{
   const char* logContext = "Apply config map";
   AbstractConfig::applyConfigMap(false);

   for(StringMapIter iter = getConfigMap()->begin(); iter != getConfigMap()->end(); )
   {
      bool unknownElement = false;

      if(iter->first == std::string("storeStorageDirectory") )
         storeTargetDirectory = iter->second;
      else
      if(iter->first == std::string("targetIdMapFile") ) // stringID to targetNumID mapping
         targetIdMapFile = iter->second;
      else
      if(iter->first == std::string("metaIdMapFile") ) // stringID to metaNumID mapping
         metaIdMapFile = iter->second;
      else
      if(iter->first == std::string("storageIdMapFile") ) // stringID to storageNumID mapping
         storageIdMapFile = iter->second;
      else
      { // unknown element occurred
         unknownElement = true;

         if(enableException)
         {
            LogContext(logContext).logErr(std::string("The config argument '") + iter->first +
               std::string("' is invalid") );
            this->isValid = false;
            return false;
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

   return true;
}



