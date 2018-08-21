#include <common/toolkit/StringTk.h>
#include <modes/ModeCreateFile.h>
#include <modes/ModeCreateDir.h>
#include <modes/ModeDisposeUnusedFiles.h>
#include <modes/ModeGenericDebug.h>
#include <modes/ModeGetEntryInfo.h>
#include <modes/ModeGetNodes.h>
#include <modes/ModeGetQuotaInfo.h>
#include <modes/ModeHashDir.h>
#include <modes/ModeHelp.h>
#include <modes/ModeIoctl.h>
#include <modes/ModeIOStat.h>
#include <modes/ModeIOTest.h>
#include <modes/ModeListTargets.h>
#include <modes/ModeMapTarget.h>
#include <modes/ModeRefreshEntryInfo.h>
#include <modes/ModeRemoveDirEntry.h>
#include <modes/ModeRemoveNode.h>
#include <modes/ModeReverseLookup.h>
#include <modes/ModeSetPattern.h>
#include <modes/ModeSetQuota.h>
#include <modes/ModeStorageBench.h>
#include <modes/ModeRemoveTarget.h>
#include <modes/migrate/ModeFind.h>
#include <modes/migrate/ModeMigrate.h>
#include <modes/mirroring/ModeAddMirrorBuddyGroup.h>
#include <modes/mirroring/ModeListMirrorBuddyGroups.h>
#include <modes/mirroring/ModeRemoveBuddyGroup.h>
#include <modes/mirroring/ModeResyncStats.h>
#include <modes/mirroring/ModeSetMetadataMirroring.h>
#include <modes/mirroring/ModeStartResync.h>
#include <modes/modeclientstats/ModeClientStats.h>
#include <toolkit/IoctlTk.h>
#include "Config.h"


#define IGNORE_CONFIG_CLIENT_VALUE(keyStr) /* to be used in applyConfigMap() */ \
   if(testConfigMapKeyMatch(iter, keyStr, addDashes) ) \
      ; \
   else

template<typename ModeT>
static std::unique_ptr<Mode> instantiate()
{
   return std::unique_ptr<Mode>(new ModeT);
}

static std::unique_ptr<Mode> instantiateUserStats()
{
   std::unique_ptr<ModeClientStats> result(new ModeClientStats);
   result->setPerUser(true);
   return std::move(result);
}

static std::unique_ptr<Mode> instantiateHelpAll()
{
   return std::unique_ptr<Mode>(new ModeHelp(true));
}

#define SIMPLE_RUN_MODE(argStr, ModeT) \
   { argStr, false, instantiate<ModeT>, ModeT::printHelp, true }
#define SIMPLE_RUN_MODE_HIDDEN(argStr, ModeT) \
   { argStr, true, instantiate<ModeT>, ModeT::printHelp, true }

RunModesElem const __RunModes[] =
{
   SIMPLE_RUN_MODE("--getentryinfo", ModeGetEntryInfo),
   SIMPLE_RUN_MODE("--setpattern", ModeSetPattern),
   SIMPLE_RUN_MODE("--listnodes", ModeGetNodes),
   SIMPLE_RUN_MODE("--disposeunused", ModeDisposeUnusedFiles),
   SIMPLE_RUN_MODE("--createfile", ModeCreateFile),
   SIMPLE_RUN_MODE("--createdir", ModeCreateDir),
   SIMPLE_RUN_MODE("--removenode", ModeRemoveNode),
   SIMPLE_RUN_MODE_HIDDEN("--iostat", ModeIOStat),
   // alias for "--iostat"
   SIMPLE_RUN_MODE("--serverstats", ModeIOStat),
   SIMPLE_RUN_MODE_HIDDEN("--iotest", ModeIOTest),
   SIMPLE_RUN_MODE("--refreshentryinfo", ModeRefreshEntryInfo),
   SIMPLE_RUN_MODE_HIDDEN("--removedirentry", ModeRemoveDirEntry),
   SIMPLE_RUN_MODE_HIDDEN("--maptarget", ModeMapTarget),
   SIMPLE_RUN_MODE_HIDDEN("--removetarget", ModeRemoveTarget),
   SIMPLE_RUN_MODE("--listtargets", ModeListTargets),
   SIMPLE_RUN_MODE_HIDDEN("--ioctl", ModeIoctl),
   SIMPLE_RUN_MODE_HIDDEN("--genericdebug", ModeGenericDebug),
   SIMPLE_RUN_MODE("--clientstats", ModeClientStats),
   // alias for "--clientstats --peruser"
   {"--userstats", false, instantiateUserStats, ModeClientStats::printHelp, true},
   SIMPLE_RUN_MODE("--find", ModeFind),
   {"--migrate", false, instantiate<ModeMigrate>, ModeMigrate::printHelpMigrate, true},
   SIMPLE_RUN_MODE_HIDDEN("--reverselookup", ModeReverseLookup),
   SIMPLE_RUN_MODE("--storagebench", ModeStorageBench),
   SIMPLE_RUN_MODE("--mirrormd", ModeSetMetadataMirroring),
   SIMPLE_RUN_MODE("--getquota" , ModeGetQuotaInfo),
   SIMPLE_RUN_MODE_HIDDEN("--hashdir",  ModeHashDir),
   SIMPLE_RUN_MODE("--setquota" , ModeSetQuota),
   SIMPLE_RUN_MODE("--addmirrorgroup", ModeAddMirrorBuddyGroup),
   SIMPLE_RUN_MODE("--listmirrorgroups", ModeListMirrorBuddyGroups),
   SIMPLE_RUN_MODE("--startresync", ModeStartResync),
   SIMPLE_RUN_MODE("--resyncstats", ModeResyncStats),
   SIMPLE_RUN_MODE_HIDDEN("--removemirrorgroup", ModeRemoveBuddyGroup),
   {"--helpall", true, instantiateHelpAll, nullptr, false},

   {nullptr, true, nullptr, nullptr, false}
};


Config::Config(int argc, char** argv) throw(InvalidConfigException) : AbstractConfig(argc, argv)
{
   initConfig(argc, argv, false, true);
}

/**
 * Determine RunMode from config.
 * If a valid RunMode exists in the config, the corresponding config element will be erased.
 */
const RunModesElem* Config::determineRunMode()
{
   StringMap* configMap = getConfigMap();

   /* test for given help argument, e.g. in case the user wants to see mode-specific help with
      arguments "--help --<mode>". */

   StringMapIter iter = configMap->find(RUNMODE_HELP_KEY_STRING);
   if(iter != configMap->end() )
   { // user did specify "--help"
      /* note: it's important to remove the help arg here, because mode help will call this again
         to find out whether user wants to see mode-specific help. */
      eraseFromConfigMap(iter);
      return nullptr;
   }

   // walk all defined modes to check whether we find any of them in the config

   for(int i=0; __RunModes[i].modeString != NULL; i++)
   {
      iter = configMap->find(__RunModes[i].modeString);
      if(iter != configMap->end() )
      { // we found a valid mode in the config
         eraseFromConfigMap(iter);
         return &__RunModes[i];
      }
   }

   // no valid mode found

   return nullptr;
}

/**
 * Sets the default values for each configurable in the configMap.
 *
 * @param addDashes true to prepend "--" to all config keys.
 */
void Config::loadDefaults(bool addDashes)
{
   AbstractConfig::loadDefaults(addDashes);

   // re-definitions
   configMapRedefine("cfgFile",                          createDefaultCfgFilename(), addDashes);

   // own definitions
   configMapRedefine("logEnabled",                       "false", addDashes);

   configMapRedefine("connInterfacesFile",               "", addDashes);

   configMapRedefine("debugRunComponentThreads",         "true", addDashes);
   configMapRedefine("debugRunStartupTests",             "false", addDashes);
   configMapRedefine("debugRunComponentTests",           "false", addDashes);
   configMapRedefine("debugRunIntegrationTests",         "false", addDashes);
   configMapRedefine("debugRunThroughputTests",          "false", addDashes);

   configMapRedefine("tuneNumWorkers",                   "3", addDashes);
   configMapRedefine("tunePreferredMetaFile",            "", addDashes);
   configMapRedefine("tunePreferredStorageFile",         "", addDashes);

   configMapRedefine("runDaemonized",                    "false", addDashes);
}

/**
 * @param addDashes true to prepend "--" to tested config keys for matching.
 */
void Config::applyConfigMap(bool enableException, bool addDashes) throw(InvalidConfigException)
{
   AbstractConfig::applyConfigMap(false, addDashes);

   for(StringMapIter iter = getConfigMap()->begin(); iter != getConfigMap()->end(); )
   {
      bool unknownElement = false;

      if (testConfigMapKeyMatch(iter, "mount", addDashes) )
      { // special argument to detect configuration file by mount point
         mount = iter->second;
         IoctlTk ioctl = IoctlTk(mount);
         if (!ioctl.getRuntimeCfgFile(&cfgFile))
            throw InvalidConfigException("Given mountpoint is invalid: " + mount);
      }
      else
      IGNORE_CONFIG_CLIENT_VALUE("logClientID")
      IGNORE_CONFIG_CLIENT_VALUE("logHelperdIP")
      if(testConfigMapKeyMatch(iter, "logEnabled", addDashes) )
         logEnabled = StringTk::strToBool(iter->second);
      else
      IGNORE_CONFIG_CLIENT_VALUE("logType")
      if(testConfigMapKeyMatch(iter, "connInterfacesFile", addDashes) )
         connInterfacesFile = iter->second;
      else
      IGNORE_CONFIG_CLIENT_VALUE("connNumCommRetries")
      IGNORE_CONFIG_CLIENT_VALUE("connUnmountRetries")
      IGNORE_CONFIG_CLIENT_VALUE("connCommRetrySecs")
      IGNORE_CONFIG_CLIENT_VALUE("connRecvNonIntrTimeoutMS")
      if(testConfigMapKeyMatch(iter, "debugRunComponentThreads", addDashes) )
         debugRunComponentThreads = StringTk::strToBool(iter->second);
      else
      if(testConfigMapKeyMatch(iter, "debugRunStartupTests", addDashes) )
         debugRunStartupTests = StringTk::strToBool(iter->second);
      else
      if(testConfigMapKeyMatch(iter, "debugRunComponentTests", addDashes) )
         debugRunComponentTests = StringTk::strToBool(iter->second);
      else
      if(testConfigMapKeyMatch(iter, "debugRunIntegrationTests", addDashes) )
         debugRunIntegrationTests = StringTk::strToBool(iter->second);
      else
      if(testConfigMapKeyMatch(iter, "debugRunThroughputTests", addDashes) )
         debugRunThroughputTests = StringTk::strToBool(iter->second);
      else
      if(testConfigMapKeyMatch(iter, "tuneNumWorkers", addDashes) )
         tuneNumWorkers = StringTk::strToUInt(iter->second);
      else
      IGNORE_CONFIG_CLIENT_VALUE("connMaxConcurrentAttempts")
      IGNORE_CONFIG_CLIENT_VALUE("tuneNumRetryWorkers")
      if(testConfigMapKeyMatch(iter, "tunePreferredMetaFile", addDashes) )
         tunePreferredMetaFile = iter->second;
      else
      if(testConfigMapKeyMatch(iter, "tunePreferredStorageFile", addDashes) )
         tunePreferredStorageFile = iter->second;
      else
      IGNORE_CONFIG_CLIENT_VALUE("tuneFileCacheType")
      IGNORE_CONFIG_CLIENT_VALUE("tunePagedIOBufSize")
      IGNORE_CONFIG_CLIENT_VALUE("tunePagedIOBufNum")
      IGNORE_CONFIG_CLIENT_VALUE("tuneFileCacheBufSize")
      IGNORE_CONFIG_CLIENT_VALUE("tuneFileCacheBufNum")
      IGNORE_CONFIG_CLIENT_VALUE("tunePageCacheValidityMS")
      IGNORE_CONFIG_CLIENT_VALUE("tuneAttribCacheValidityMS")
      IGNORE_CONFIG_CLIENT_VALUE("tuneDirSubentryCacheValidityMS")
      IGNORE_CONFIG_CLIENT_VALUE("tuneFileSubentryCacheValidityMS")
      IGNORE_CONFIG_CLIENT_VALUE("tuneMaxWriteWorks")
      IGNORE_CONFIG_CLIENT_VALUE("tuneMaxReadWorks")
      IGNORE_CONFIG_CLIENT_VALUE("tuneAllowMultiSetWrite")
      IGNORE_CONFIG_CLIENT_VALUE("tuneAllowMultiSetRead")
      IGNORE_CONFIG_CLIENT_VALUE("tunePathBufSize")
      IGNORE_CONFIG_CLIENT_VALUE("tunePathBufNum")
      IGNORE_CONFIG_CLIENT_VALUE("tuneMaxReadWriteNum")
      IGNORE_CONFIG_CLIENT_VALUE("tuneMaxReadWriteNodesNum")
      IGNORE_CONFIG_CLIENT_VALUE("tuneMsgBufSize")
      IGNORE_CONFIG_CLIENT_VALUE("tuneMsgBufNum")
      IGNORE_CONFIG_CLIENT_VALUE("tuneRemoteFSync")
      IGNORE_CONFIG_CLIENT_VALUE("tuneUseGlobalFileLocks")
      IGNORE_CONFIG_CLIENT_VALUE("tuneRefreshOnGetAttr")
      IGNORE_CONFIG_CLIENT_VALUE("tuneInodeBlockBits")
      IGNORE_CONFIG_CLIENT_VALUE("tuneInodeBlockSize")
      IGNORE_CONFIG_CLIENT_VALUE("tuneMaxClientMirrorSize") // was removed, kept here for compat
      IGNORE_CONFIG_CLIENT_VALUE("tuneEarlyCloseResponse")
      IGNORE_CONFIG_CLIENT_VALUE("tuneUseGlobalAppendLocks")
      IGNORE_CONFIG_CLIENT_VALUE("tuneUseBufferedAppend")
      IGNORE_CONFIG_CLIENT_VALUE("tuneStatFsCacheSecs")
      IGNORE_CONFIG_CLIENT_VALUE("tuneCoherentBuffers")
      IGNORE_CONFIG_CLIENT_VALUE("sysInodeIDStyle")
      IGNORE_CONFIG_CLIENT_VALUE("sysCreateHardlinksAsSymlinks")
      IGNORE_CONFIG_CLIENT_VALUE("sysMountSanityCheckMS")
      IGNORE_CONFIG_CLIENT_VALUE("sysSyncOnClose")
      IGNORE_CONFIG_CLIENT_VALUE("sysSessionCheckOnClose")
      IGNORE_CONFIG_CLIENT_VALUE("sysTargetOfflineTimeoutSecs")
      IGNORE_CONFIG_CLIENT_VALUE("sysXAttrsEnabled")
      IGNORE_CONFIG_CLIENT_VALUE("sysACLsEnabled")

      IGNORE_CONFIG_CLIENT_VALUE("quotaEnabled")
      IGNORE_CONFIG_CLIENT_VALUE("sysRenameEbusyAsXdev")
      if(testConfigMapKeyMatch(iter, "runDaemonized", addDashes) )
         runDaemonized = StringTk::strToBool(iter->second);
      else
      { // unknown element occurred
         unknownElement = true;

         if(enableException)
         {
            throw InvalidConfigException(
               std::string("The config argument '") + iter->first + std::string("' is invalid") );
         }
      }

      // advance iterator (and remove handled element)

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

