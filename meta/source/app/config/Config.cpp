#include <common/nodes/TargetCapacityPools.h>
#include <common/system/System.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/UnitTk.h>
#include "Config.h"

#define CONFIG_DEFAULT_CFGFILENAME "/etc/beegfs/beegfs-meta.conf"

#define TARGETCHOOSERTYPE_RANDOMIZED_STR        "randomized"
#define TARGETCHOOSERTYPE_ROUNDROBIN_STR        "roundrobin"
#define TARGETCHOOSERTYPE_RANDOMROBIN_STR       "randomrobin"
#define TARGETCHOOSERTYPE_RANDOMINTERNODE_STR   "randominternode"
#define TARGETCHOOSERTYPE_RANDOMINTRANODE_STR   "randomintranode"


Config::Config(int argc, char** argv):
      AbstractConfig(argc, argv)
{
   sysTargetAttachmentMap = NULL;

   initConfig(argc, argv, true);
}

Config::~Config()
{
   SAFE_DELETE(sysTargetAttachmentMap);
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
   configMapRedefine("cfgFile",                    "");
   configMapRedefine("connMaxInternodeNum",        "16");

   // own definitions
   configMapRedefine("connInterfacesFile",         "");
   configMapRedefine("connInterfacesList",         "");

   configMapRedefine("storeMetaDirectory",         "");
   configMapRedefine("storeFsUUID",                "");
   configMapRedefine("storeAllowFirstRunInit",     "true");
   configMapRedefine("storeUseExtendedAttribs",    "true");
   configMapRedefine("storeSelfHealEmptyFiles",    "true");

   configMapRedefine("storeClientXAttrs",          "false");
   configMapRedefine("storeClientACLs",            "false");

   configMapRedefine("sysTargetAttachmentFile",    "");
   configMapRedefine("tuneNumStreamListeners",     "1");
   configMapRedefine("tuneNumWorkers",             "0");
   configMapRedefine("tuneWorkerBufSize",          "1m");
   configMapRedefine("tuneNumCommSlaves",          "0");
   configMapRedefine("tuneCommSlaveBufSize",       "1m");
   configMapRedefine("tuneDefaultChunkSize",       "512k");
   configMapRedefine("tuneDefaultNumStripeTargets","4");
   configMapRedefine("tuneProcessFDLimit",         "50000");
   configMapRedefine("tuneWorkerNumaAffinity",     "false");
   configMapRedefine("tuneListenerNumaAffinity",   "false");
   configMapRedefine("tuneBindToNumaZone",         "");
   configMapRedefine("tuneListenerPrioShift",      "-1");
   configMapRedefine("tuneDirMetadataCacheLimit",  "1024");
   configMapRedefine("tuneTargetChooser",          TARGETCHOOSERTYPE_RANDOMIZED_STR);
   configMapRedefine("tuneLockGrantWaitMS",        "333");
   configMapRedefine("tuneLockGrantNumRetries",    "15");
   configMapRedefine("tuneRotateMirrorTargets",    "false");
   configMapRedefine("tuneEarlyUnlinkResponse",    "true");
   configMapRedefine("tuneUsePerUserMsgQueues",    "false");
   configMapRedefine("tuneUseAggressiveStreamPoll","false");
   configMapRedefine("tuneNumResyncSlaves",        "12");
   configMapRedefine("tuneMirrorTimestamps",        "true");
   configMapRedefine("tuneDisposalGCPeriod",       "0");

   configMapRedefine("quotaEarlyChownResponse",    "true");
   configMapRedefine("quotaEnableEnforcement",     "false");

   configMapRedefine("sysTargetOfflineTimeoutSecs","180");
   configMapRedefine("sysAllowUserSetPattern",     "false");
   configMapRedefine("sysFileEventLogTarget",      "");

   configMapRedefine("runDaemonized",              "false");

   configMapRedefine("pidFile",                    "");
}

/**
 * @param addDashes currently usused
 */
void Config::applyConfigMap(bool enableException, bool addDashes)
{
   AbstractConfig::applyConfigMap(false);

   for (StringMapIter iter = configMap.begin(); iter != configMap.end();)
   {
      bool unknownElement = false;

      if (iter->first == std::string("logType"))
      {
         if (iter->second == "syslog")
         {
            logType = LogType_SYSLOG;
         }
         else if (iter->second == "logfile")
         {
            logType =  LogType_LOGFILE;
         }
         else
         {
            throw InvalidConfigException("The value of config argument logType is invalid.");
         }
      }
      else if (iter->first == std::string("connInterfacesFile"))
         connInterfacesFile = iter->second;
      else if (iter->first == std::string("connInterfacesList"))
         connInterfacesList = iter->second;
      else if (iter->first == std::string("storeMetaDirectory"))
         storeMetaDirectory = iter->second;
      else if (iter->first == std::string("storeFsUUID"))
         storeFsUUID = iter->second;
      else if (iter->first == std::string("storeAllowFirstRunInit"))
         storeAllowFirstRunInit = StringTk::strToBool(iter->second);
      else if (iter->first == std::string("storeUseExtendedAttribs"))
         storeUseExtendedAttribs = StringTk::strToBool(iter->second);
      else if (iter->first == std::string("storeSelfHealEmptyFiles"))
         storeSelfHealEmptyFiles = StringTk::strToBool(iter->second);
      else if (iter->first == std::string("storeClientXAttrs"))
         storeClientXAttrs = StringTk::strToBool(iter->second);
      else if (iter->first == std::string("storeClientACLs"))
         storeClientACLs = StringTk::strToBool(iter->second);
      else if (iter->first == std::string("sysTargetAttachmentFile"))
         sysTargetAttachmentFile = iter->second;
      else if (iter->first == std::string("tuneNumStreamListeners"))
         tuneNumStreamListeners = StringTk::strToUInt(iter->second);
      else if (iter->first == std::string("tuneNumWorkers"))
         tuneNumWorkers = StringTk::strToUInt(iter->second);
      else if (iter->first == std::string("tuneWorkerBufSize"))
         tuneWorkerBufSize = UnitTk::strHumanToInt64(iter->second);
      else if (iter->first == std::string("tuneNumCommSlaves"))
         tuneNumCommSlaves = StringTk::strToUInt(iter->second);
      else if (iter->first == std::string("tuneCommSlaveBufSize"))
         tuneCommSlaveBufSize = UnitTk::strHumanToInt64(iter->second);
      else if (iter->first == std::string("tuneDefaultChunkSize"))
         tuneDefaultChunkSize = UnitTk::strHumanToInt64(iter->second);
      else if (iter->first == std::string("tuneDefaultNumStripeNodes"))  // old "...Nodes" kept for compat
         tuneDefaultNumStripeTargets = StringTk::strToUInt(iter->second);
      else if (iter->first == std::string("tuneDefaultNumStripeTargets"))
         tuneDefaultNumStripeTargets = StringTk::strToUInt(iter->second);
      else if (iter->first == std::string("tuneProcessFDLimit"))
         tuneProcessFDLimit = StringTk::strToUInt(iter->second);
      else if (iter->first == std::string("tuneWorkerNumaAffinity"))
         tuneWorkerNumaAffinity = StringTk::strToBool(iter->second);
      else if (iter->first == std::string("tuneListenerNumaAffinity"))
         tuneListenerNumaAffinity = StringTk::strToBool(iter->second);
      else if (iter->first == std::string("tuneBindToNumaZone"))
      {
         if (iter->second.empty()) // not defined => disable
            tuneBindToNumaZone = -1; // -1 means disable binding
         else
            tuneBindToNumaZone = StringTk::strToInt(iter->second);
      }
      else if (iter->first == std::string("tuneListenerPrioShift"))
         tuneListenerPrioShift = StringTk::strToInt(iter->second);
      else if (iter->first == std::string("tuneDirMetadataCacheLimit"))
         tuneDirMetadataCacheLimit = StringTk::strToUInt(iter->second);
      else if (iter->first == std::string("tuneTargetChooser"))
         tuneTargetChooser = iter->second;
      else if (iter->first == std::string("tuneLockGrantWaitMS"))
         tuneLockGrantWaitMS = StringTk::strToUInt(iter->second);
      else if (iter->first == std::string("tuneLockGrantNumRetries"))
         tuneLockGrantNumRetries = StringTk::strToUInt(iter->second);
      else if (iter->first == std::string("tuneRotateMirrorTargets"))
         tuneRotateMirrorTargets = StringTk::strToBool(iter->second);
      else if (iter->first == std::string("tuneEarlyUnlinkResponse"))
         tuneEarlyUnlinkResponse = StringTk::strToBool(iter->second);
      else if (iter->first == std::string("tuneUseAggressiveStreamPoll"))
         tuneUseAggressiveStreamPoll = StringTk::strToBool(iter->second);
      else if (iter->first == std::string("tuneNumResyncSlaves"))
         this->tuneNumResyncSlaves = StringTk::strToUInt(iter->second);
      else if (iter->first == std::string("quotaEarlyChownResponse"))
         quotaEarlyChownResponse = StringTk::strToBool(iter->second);
      else if (iter->first == std::string("quotaEnableEnforcement"))
         quotaEnableEnforcement = StringTk::strToBool(iter->second);
      else if (iter->first == std::string("sysTargetOfflineTimeoutSecs"))
      {
         sysTargetOfflineTimeoutSecs = StringTk::strToUInt(iter->second);

         if (sysTargetOfflineTimeoutSecs < 30)
         {
            throw InvalidConfigException("Invalid sysTargetOfflineTimeoutSecs value "
                  + iter->second + " (must be at least 30)");
         }
      }
      else if (iter->first == std::string("sysAllowUserSetPattern"))
         sysAllowUserSetPattern = StringTk::strToBool(iter->second.c_str());
      else if (iter->first == std::string("tuneUsePerUserMsgQueues"))
         tuneUsePerUserMsgQueues = StringTk::strToBool(iter->second);
      else if (iter->first == std::string("tuneMirrorTimestamps"))
         tuneMirrorTimestamps = StringTk::strToBool(iter->second);
      else if(iter->first == std::string("tuneDisposalGCPeriod"))
          tuneDisposalGCPeriod = StringTk::strToUInt(iter->second);
      else if (iter->first == std::string("sysFileEventLogTarget"))
         sysFileEventLogTarget = iter->second;
      else if (iter->first == std::string("runDaemonized"))
         runDaemonized = StringTk::strToBool(iter->second);
      else if (iter->first == std::string("pidFile"))
         pidFile = iter->second;
      else
      {
         // unknown element occurred
         unknownElement = true;

         if (enableException)
         {
            throw InvalidConfigException("The config argument '" + iter->first + "' is invalid");
         }
      }

      if (unknownElement)
      {
         // just skip the unknown element
         iter++;
      }
      else
      {
         // remove this element from the map
         iter = eraseFromConfigMap(iter);
      }
   }
}

void Config::initImplicitVals()
{
   // tuneNumWorkers (note: twice the number of cpu cores is default, but at least 4)
   if(!tuneNumWorkers)
      tuneNumWorkers = BEEGFS_MAX(System::getNumOnlineCPUs()*2, 4);

   // tuneNumCommSlaves
   if(!tuneNumCommSlaves)
      tuneNumCommSlaves = tuneNumWorkers * 2;

   // tuneTargetChooserNum
   initTuneTargetChooserNum();

   // connInterfacesList(/File)
   AbstractConfig::initInterfacesList(connInterfacesFile, connInterfacesList);

   AbstractConfig::initSocketBufferSizes();

   // connAuthHash
   AbstractConfig::initConnAuthHash(connAuthFile, &connAuthHash);

   // sysTargetAttachmentMap
   initSysTargetAttachmentMap();
}

void Config::initSysTargetAttachmentMap()
{
   if(sysTargetAttachmentFile.empty() )
      return; // no file given => nothing to do here

   // check if file exists

   if(!StorageTk::pathExists(sysTargetAttachmentFile) )
      throw InvalidConfigException("sysTargetAttachmentFile not found: " +
         sysTargetAttachmentFile);

   // load as string map

   StringMap attachmentStrMap;

   MapTk::loadStringMapFromFile(sysTargetAttachmentFile.c_str(), &attachmentStrMap);

   // convert from string map to target map

   sysTargetAttachmentMap = new TargetMap();

   for(StringMapCIter iter = attachmentStrMap.begin(); iter != attachmentStrMap.end(); iter++)
   {
      (*sysTargetAttachmentMap)[StringTk::strToUInt(iter->first)] =
         NumNodeID(StringTk::strToUInt(iter->second) );
   }
}

void Config::initTuneTargetChooserNum()
{
   if (this->tuneTargetChooser == TARGETCHOOSERTYPE_RANDOMIZED_STR)
      this->tuneTargetChooserNum = TargetChooserType_RANDOMIZED;
   else if (this->tuneTargetChooser == TARGETCHOOSERTYPE_ROUNDROBIN_STR)
      this->tuneTargetChooserNum = TargetChooserType_ROUNDROBIN;
   else if (this->tuneTargetChooser == TARGETCHOOSERTYPE_RANDOMROBIN_STR)
      this->tuneTargetChooserNum = TargetChooserType_RANDOMROBIN;
   else if (this->tuneTargetChooser == TARGETCHOOSERTYPE_RANDOMINTERNODE_STR)
      this->tuneTargetChooserNum = TargetChooserType_RANDOMINTERNODE;
   // Don't allow RANDOMINTRANODE Target Chooser
   else
   {
      // invalid chooser specified
      throw InvalidConfigException("Invalid storage target chooser specified: " 
            + tuneTargetChooser);
   }
}

std::string Config::createDefaultCfgFilename() const
{
   struct stat statBuf;

   const int statRes = stat(CONFIG_DEFAULT_CFGFILENAME, &statBuf);

   if(!statRes && S_ISREG(statBuf.st_mode) )
      return CONFIG_DEFAULT_CFGFILENAME; // there appears to be a config file

   return ""; // no default file otherwise
}

