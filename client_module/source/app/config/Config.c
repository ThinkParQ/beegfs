#include <app/log/Logger.h>
#include <common/toolkit/HashTk.h>
#include <common/toolkit/StringTk.h>
#include "Config.h"

#include <linux/fs.h>
#include <asm/uaccess.h>


#define CONFIG_DEFAULT_CFGFILENAME     "/etc/beegfs/beegfs-client.conf"
#define CONFIG_FILE_COMMENT_CHAR       '#'
#define CONFIG_FILE_MAX_LINE_LENGTH    1024
#define CONFIG_ERR_BUF_LENGTH          1024
#define CONFIG_AUTHFILE_READSIZE       1024 // max amount of data that we read from auth file
#define CONFIG_AUTHFILE_MINSIZE        4 // at least 2, because we compute two 32bit hashes

#define CONFIG_CONN_RDMA_BUFNUM_MIN    3 // required by the IBVSocket logic and protocol
#define CONFIG_CONN_RDMA_NONE_STR      "none"
#define CONFIG_CONN_RDMA_DEFAULT_STR   "default"
#define CONFIG_CONN_RDMA_PAGE_STR      "page"
#define CONFIG_CONN_RDMA_DEFAULT       -1

#define FILECACHETYPE_NONE_STR      "none"
#define FILECACHETYPE_BUFFERED_STR  "buffered"
#define FILECACHETYPE_PAGED_STR     "paged"
#define FILECACHETYPE_NATIVE_STR    "native"

#define LOGGERTYPE_HELPERD_STR      "helperd"
#define LOGGERTYPE_SYSLOG_STR       "syslog"

#define EVENTLOGMASK_NONE "none"
#define EVENTLOGMASK_FLUSH "flush"
#define EVENTLOGMASK_TRUNC "trunc"
#define EVENTLOGMASK_SETATTR "setattr"
#define EVENTLOGMASK_CLOSE "close"
#define EVENTLOGMASK_LINK_OP "link-op"
#define EVENTLOGMASK_READ "read"

#define IGNORE_CONFIG_VALUE(compareStr) /* to be used in applyConfigMap() */ \
   if(!strcmp(keyStr, compareStr) ) \
      ; \
   else



static bool __Config_readLineFromFile(struct file* cfgFile,
   char* buf, size_t bufLen, bool* outEndOfFile);



static size_t Config_fs_read(struct file *file, char *buf, size_t size, loff_t *pos)
{
   size_t readRes;

#if defined(KERNEL_HAS_KERNEL_READ)
   readRes = kernel_read(file, buf, size, pos);
#else

   WITH_PROCESS_CONTEXT {
      readRes = vfs_read(file, buf, size, pos);
   }

#endif

   return readRes;
}

static bool assignKeyIfNotZero(const char* key, const char* strVal, int* const intVal) {
   const int tempVal = StringTk_strToInt(strVal);
   if (tempVal <= 0 || intVal == NULL) {
      return false;
   }

   *intVal = tempVal;
   return true;
}

/**
 * @param mountConfig will be copied (not owned by this object)
 */
bool Config_init(Config* this, MountConfig* mountConfig)
{
   // init configurable strings
   this->cfgFile = NULL;
   this->logHelperdIP = NULL;
   this->logType = NULL;
   this->connInterfacesFile = NULL;
   this->connRDMAInterfacesFile = NULL;
   this->connNetFilterFile = NULL;
   this->connAuthFile = NULL;
   this->connMessagingTimeouts = NULL;
   this->connRDMATimeouts = NULL;
   this->connTcpOnlyFilterFile = NULL;
   this->tunePreferredMetaFile = NULL;
   this->tunePreferredStorageFile = NULL;
   this->tuneFileCacheType = NULL;
   this->sysMgmtdHost = NULL;
   this->sysInodeIDStyle = NULL;
   this->connInterfacesList = NULL;
   this->connRDMAKeyType = NULL;

   return _Config_initConfig(this, mountConfig, true);
}

Config* Config_construct(MountConfig* mountConfig)
{
   Config* this = kmalloc(sizeof(Config), GFP_NOFS);

   if(!this ||
      !Config_init(this, mountConfig) )
   {
      kfree(this);
      return NULL;
   }

   return this;
}

void Config_uninit(Config* this)
{
   SAFE_KFREE(this->cfgFile);
   SAFE_KFREE(this->logHelperdIP);
   SAFE_KFREE(this->logType);
   SAFE_KFREE(this->connInterfacesFile);
   SAFE_KFREE(this->connRDMAInterfacesFile);
   SAFE_KFREE(this->connNetFilterFile);
   SAFE_KFREE(this->connAuthFile);
   SAFE_KFREE(this->connTcpOnlyFilterFile);
   SAFE_KFREE(this->connMessagingTimeouts);
   SAFE_KFREE(this->connRDMATimeouts);
   SAFE_KFREE(this->tunePreferredMetaFile);
   SAFE_KFREE(this->tunePreferredStorageFile);
   SAFE_KFREE(this->tuneFileCacheType);
   SAFE_KFREE(this->sysMgmtdHost);
   SAFE_KFREE(this->sysInodeIDStyle);
   SAFE_KFREE(this->connInterfacesList);
   SAFE_KFREE(this->connRDMAKeyType);

   StrCpyMap_uninit(&this->configMap);
}

void Config_destruct(Config* this)
{
   Config_uninit(this);

   kfree(this);
}

bool _Config_initConfig(Config* this, MountConfig* mountConfig, bool enableException)
{
   StrCpyMap_init(&this->configMap);

   // load and apply args to see whether we have a cfgFile
   _Config_loadDefaults(this);
   __Config_loadFromMountConfig(this, mountConfig);

   if(!_Config_applyConfigMap(this, enableException) )
      goto error;

   if(this->cfgFile && strlen(this->cfgFile) )
   { // there is a config file specified
      // start over again and include the config file this time
      StrCpyMap_clear(&this->configMap);

      _Config_loadDefaults(this);
      if(!__Config_loadFromFile(this, this->cfgFile) )
         goto error;

      __Config_loadFromMountConfig(this, mountConfig);

      if(!_Config_applyConfigMap(this, enableException) )
         goto error;

      if (this->connMaxInternodeNum > 0xffff)
         goto error;
   }

   return __Config_initImplicitVals(this);

error:
   StrCpyMap_uninit(&this->configMap);
   return false;
}

StrCpyMapIter _Config_eraseFromConfigMap(Config* this, StrCpyMapIter* iter)
{
   char* nextKey;
   StrCpyMapIter nextIter;

   nextIter = *iter;
   StrCpyMapIter_next(&nextIter);

   if(StrCpyMapIter_end(&nextIter) )
   { // no next element in the map
      StrCpyMap_erase(&this->configMap, StrCpyMapIter_key(iter) );
      return nextIter;
   }

   nextKey = StrCpyMapIter_key(&nextIter);
   StrCpyMap_erase(&this->configMap, StrCpyMapIter_key(iter) );

   return StrCpyMap_find(&this->configMap, nextKey);
}

void _Config_loadDefaults(Config* this)
{
   /**
    * **IMPORTANT**: Don't forget to add new values also to the BEEGFS_ONLINE_CFG and BEEGFS_FSCK
    * ignored config values!
    */

   _Config_configMapRedefine(this, "cfgFile",                          "");

   _Config_configMapRedefine(this, "logLevel",                         "3");
   _Config_configMapRedefine(this, "logClientID",                      "false");
   _Config_configMapRedefine(this, "logHelperdIP",                     "127.0.0.1");
   _Config_configMapRedefine(this, "logType",                          LOGGERTYPE_HELPERD_STR);

   _Config_configMapRedefine(this, "connPortShift",                    "0");
   _Config_configMapRedefine(this, "connClientPortUDP",                "8004");
   _Config_configMapRedefine(this, "connMgmtdPortUDP",                 "8008");
   _Config_configMapRedefine(this, "connHelperdPortTCP",               "8006");
   _Config_configMapRedefine(this, "connMgmtdPortTCP",                 "8008");
   _Config_configMapRedefine(this, "connUseRDMA",                      "true");
   _Config_configMapRedefine(this, "connTCPFallbackEnabled",           "true");
   _Config_configMapRedefine(this, "connMaxInternodeNum",              "8");
   _Config_configMapRedefine(this, "connInterfacesFile",               "");
   _Config_configMapRedefine(this, "connInterfacesList",              "");
   _Config_configMapRedefine(this, "connRDMAInterfacesFile",           "");
   _Config_configMapRedefine(this, "connFallbackExpirationSecs",       "900");
   _Config_configMapRedefine(this, "connCommRetrySecs",                "600");
   _Config_configMapRedefine(this, "connUnmountRetries",               "true");
   _Config_configMapRedefine(this, "connTCPRcvBufSize",                "0");
   _Config_configMapRedefine(this, "connUDPRcvBufSize",                "0");
   _Config_configMapRedefine(this, "connRDMABufSize",                  "8192");
   _Config_configMapRedefine(this, "connRDMAFragmentSize",             CONFIG_CONN_RDMA_PAGE_STR);
   _Config_configMapRedefine(this, "connRDMABufNum",                   "70");
   _Config_configMapRedefine(this, "connRDMAMetaBufSize",              CONFIG_CONN_RDMA_DEFAULT_STR);
   _Config_configMapRedefine(this, "connRDMAMetaFragmentSize",         CONFIG_CONN_RDMA_DEFAULT_STR);
   _Config_configMapRedefine(this, "connRDMAMetaBufNum",               CONFIG_CONN_RDMA_DEFAULT_STR);
   _Config_configMapRedefine(this, "connRDMATypeOfService",            "0");
   _Config_configMapRedefine(this, "connRDMAKeyType",                  RDMAKEYTYPE_UNSAFE_GLOBAL_STR);
   _Config_configMapRedefine(this, "connNetFilterFile",                "");
   _Config_configMapRedefine(this, "connMaxConcurrentAttempts",        "0");
   _Config_configMapRedefine(this, "connAuthFile",                     "");
   _Config_configMapRedefine(this, "connDisableAuthentication",        "false");
   _Config_configMapRedefine(this, "connTcpOnlyFilterFile",            "");

   /* connMessagingTimeouts: default to zero, indicating that constants
    * specified in Common.h are used.
    */
   _Config_configMapRedefine(this, "connMessagingTimeouts",            "0,0,0");
   // connRDMATimeouts: zero values are interpreted as the defaults specified in IBVSocket.c
   _Config_configMapRedefine(this, "connRDMATimeouts",                 "0,0,0,0,0");

   _Config_configMapRedefine(this, "tunePreferredMetaFile",            "");
   _Config_configMapRedefine(this, "tunePreferredStorageFile",         "");
   _Config_configMapRedefine(this, "tuneFileCacheType",                FILECACHETYPE_BUFFERED_STR);
   _Config_configMapRedefine(this, "tuneFileCacheBufSize",             "524288");
   _Config_configMapRedefine(this, "tuneFileCacheBufNum",              "0");
   _Config_configMapRedefine(this, "tunePageCacheValidityMS",          "2000000000");
   _Config_configMapRedefine(this, "tuneDirSubentryCacheValidityMS",   "1000");
   _Config_configMapRedefine(this, "tuneFileSubentryCacheValidityMS",  "0");
   _Config_configMapRedefine(this, "tuneENOENTCacheValidityMS",        "0");
   _Config_configMapRedefine(this, "tunePathBufSize",                  "4096");
   _Config_configMapRedefine(this, "tunePathBufNum",                   "8");
   _Config_configMapRedefine(this, "tuneMsgBufSize",                   "65536");
   _Config_configMapRedefine(this, "tuneMsgBufNum",                    "0");
   _Config_configMapRedefine(this, "tuneRemoteFSync",                  "true");
   _Config_configMapRedefine(this, "tuneUseGlobalFileLocks",           "false");
   _Config_configMapRedefine(this, "tuneRefreshOnGetAttr",             "false");
   _Config_configMapRedefine(this, "tuneInodeBlockBits",               "19");
   _Config_configMapRedefine(this, "tuneEarlyCloseResponse",           "false");
   _Config_configMapRedefine(this, "tuneUseGlobalAppendLocks",         "false");
   _Config_configMapRedefine(this, "tuneUseBufferedAppend",            "true");
   _Config_configMapRedefine(this, "tuneStatFsCacheSecs",              "10");
   _Config_configMapRedefine(this, "tuneCoherentBuffers",              "true");

   _Config_configMapRedefine(this, "sysMgmtdHost",                     "");
   _Config_configMapRedefine(this, "sysInodeIDStyle",                  INODEIDSTYLE_DEFAULT);
   _Config_configMapRedefine(this, "sysCreateHardlinksAsSymlinks",     "false");
   _Config_configMapRedefine(this, "sysMountSanityCheckMS",            "11000");
   _Config_configMapRedefine(this, "sysSyncOnClose",                   "false");
   _Config_configMapRedefine(this, "sysSessionCheckOnClose",           "false");
   _Config_configMapRedefine(this, "sysSessionChecksEnabled",          "true");

   _Config_configMapRedefine(this, "sysUpdateTargetStatesSecs",        "30");
   _Config_configMapRedefine(this, "sysTargetOfflineTimeoutSecs",      "900");
   // Note: The default here is intentionally set to double the value from the server config.
   // This ensures that the servers push their state twice during one Mgmtd InternodeSyncer run,
   // but the client only needs to fetch the states once during that period.

   _Config_configMapRedefine(this, "sysXAttrsEnabled",                 "false");
   _Config_configMapRedefine(this, "sysXAttrsCheckCapabilities",       "never");
   _Config_configMapRedefine(this, "sysACLsEnabled",                   "false");

   _Config_configMapRedefine(this, "quotaEnabled",                     "false");
   _Config_configMapRedefine(this, "sysFileEventLogMask",              EVENTLOGMASK_NONE);
   _Config_configMapRedefine(this, "sysRenameEbusyAsXdev",             "false");

   _Config_configMapRedefine(this, "remapConnectionFailureStatus",     "0");

   _Config_configMapRedefine(this, "sysNoEnterpriseFeatureMsg",        "false");
}

bool _Config_applyConfigMap(Config* this, bool enableException)
{
   /**
    * **IMPORTANT**: Don't forget to add new values also to the BEEGFS_ONLINE_CFG and BEEGFS_FSCK
    * ignored config values!
    */


   StrCpyMapIter iter = StrCpyMap_begin(&this->configMap);

   while(!StrCpyMapIter_end(&iter) )
   {
      bool unknownElement = false;

      char* keyStr = StrCpyMapIter_key(&iter);
      char* valueStr = StrCpyMapIter_value(&iter);

      if(!strcmp(keyStr, "cfgFile") )
      {
         SAFE_KFREE(this->cfgFile);
         this->cfgFile = StringTk_strDup(valueStr );
      }
      else
      if(!strcmp(keyStr, "logLevel") )
         this->logLevel = StringTk_strToInt(valueStr);
      else
      IGNORE_CONFIG_VALUE("logNoDate")
      if(!strcmp(keyStr, "logClientID") )
         this->logClientID = StringTk_strToBool(valueStr);
      else
      if(!strcmp(keyStr, "logHelperdIP") )
      {
         SAFE_KFREE(this->logHelperdIP);
         this->logHelperdIP = StringTk_strDup(valueStr);
      }
      else
      IGNORE_CONFIG_VALUE("logStdFile")
      IGNORE_CONFIG_VALUE("logNumLines")
      IGNORE_CONFIG_VALUE("logNumRotatedFiles")
      if(!strcmp(keyStr, "logType") )
      {
         SAFE_KFREE(this->logType);
         this->logType = StringTk_strDup(valueStr);
      }
      else
      if(!strcmp(keyStr, "connPortShift") )
         this->connPortShift = StringTk_strToInt(valueStr);
      else
      if(!strcmp(keyStr, "connClientPortUDP") )
      {
         if (!assignKeyIfNotZero(keyStr, valueStr, &this->connClientPortUDP))
            goto bad_config_elem;
      }
      else
      if(!strcmp(keyStr, "connMgmtdPortUDP") )
      {
         if (!assignKeyIfNotZero(keyStr, valueStr, &this->connMgmtdPortUDP))
            goto bad_config_elem;
      }
      else
      if(!strcmp(keyStr, "connHelperdPortTCP") )
      {
         if (!assignKeyIfNotZero(keyStr, valueStr, &this->connHelperdPortTCP))
            goto bad_config_elem;
      }
      else
      if(!strcmp(keyStr, "connMgmtdPortTCP") )
      {
         if (!assignKeyIfNotZero(keyStr, valueStr, &this->connMgmtdPortTCP))
            goto bad_config_elem;
      }
      else
      if(!strcmp(keyStr, "connUseRDMA") )
         this->connUseRDMA = StringTk_strToBool(valueStr);
      else
      if(!strcmp(keyStr, "connTCPFallbackEnabled") )
         this->connTCPFallbackEnabled = StringTk_strToBool(valueStr);
      else
      if(!strcmp(keyStr, "connRDMATypeOfService") )
         this->connRDMATypeOfService = StringTk_strToInt(valueStr);
      else
      if(!strcmp(keyStr, "connMaxInternodeNum") )
         this->connMaxInternodeNum = StringTk_strToUInt(valueStr);
      else
      if(!strcmp(keyStr, "connInterfacesFile") )
      {
         SAFE_KFREE(this->connInterfacesFile);
         this->connInterfacesFile = StringTk_strDup(valueStr);
      }
      else
      if(!strcmp(keyStr, "connInterfacesList") )
      {
         SAFE_KFREE(this->connInterfacesList);
         this->connInterfacesList = StringTk_strDup(valueStr);
      }
      else
      if(!strcmp(keyStr, "connRDMAInterfacesFile") )
      {
         SAFE_KFREE(this->connRDMAInterfacesFile);
         this->connRDMAInterfacesFile = StringTk_strDup(valueStr);
      }
      else
      IGNORE_CONFIG_VALUE("connNonPrimaryExpiration")
      if(!strcmp(keyStr, "connFallbackExpirationSecs") )
         this->connFallbackExpirationSecs = StringTk_strToUInt(valueStr);
      else
      if(!strcmp(keyStr, "connCommRetrySecs") )
         this->connCommRetrySecs = StringTk_strToUInt(valueStr);
      else
      IGNORE_CONFIG_VALUE("connNumCommRetries") // auto-generated based on connCommRetrySecs
      if(!strcmp(keyStr, "connUnmountRetries") )
         this->connUnmountRetries = StringTk_strToBool(valueStr);
      else
      if(!strcmp(keyStr, "connTCPRcvBufSize") )
         this->connTCPRcvBufSize = StringTk_strToInt(valueStr);
      else
      if(!strcmp(keyStr, "connUDPRcvBufSize") )
         this->connUDPRcvBufSize = StringTk_strToInt(valueStr);
      else
      if(!strcmp(keyStr, "connRDMABufSize") )
         this->connRDMABufSize = StringTk_strToUInt(valueStr);
      else
      if(!strcmp(keyStr, "connRDMAFragmentSize") )
      {
         if (!strcmp(valueStr, CONFIG_CONN_RDMA_PAGE_STR))
            this->connRDMAFragmentSize = PAGE_SIZE;
         else if (!strcmp(valueStr, CONFIG_CONN_RDMA_NONE_STR))
            this->connRDMAFragmentSize = 0;
         else
            this->connRDMAFragmentSize = StringTk_strToUInt(valueStr);
      }
      else
      if(!strcmp(keyStr, "connRDMABufNum") )
         this->connRDMABufNum = StringTk_strToUInt(valueStr);
      else
      if(!strcmp(keyStr, "connRDMAMetaBufSize") )
      {
         if (!strcmp(valueStr, CONFIG_CONN_RDMA_DEFAULT_STR))
            this->connRDMAMetaBufSize = CONFIG_CONN_RDMA_DEFAULT;
         else
            this->connRDMAMetaBufSize = StringTk_strToInt(valueStr);
      }
      else
      if(!strcmp(keyStr, "connRDMAMetaFragmentSize") )
      {
         if (!strcmp(valueStr, CONFIG_CONN_RDMA_PAGE_STR))
            this->connRDMAMetaFragmentSize = PAGE_SIZE;
         else if (!strcmp(valueStr, CONFIG_CONN_RDMA_NONE_STR))
            this->connRDMAMetaFragmentSize = 0;
         else if (!strcmp(valueStr, CONFIG_CONN_RDMA_DEFAULT_STR))
            this->connRDMAMetaFragmentSize = CONFIG_CONN_RDMA_DEFAULT;
         else
            this->connRDMAMetaFragmentSize = StringTk_strToInt(valueStr);
      }
      else
      if(!strcmp(keyStr, "connRDMAMetaBufNum") )
      {
         if (!strcmp(valueStr, CONFIG_CONN_RDMA_DEFAULT_STR))
            this->connRDMAMetaBufNum = CONFIG_CONN_RDMA_DEFAULT;
         else
            this->connRDMAMetaBufNum = StringTk_strToUInt(valueStr);
      }
      else
      if(!strcmp(keyStr, "connRDMAKeyType") )
      {
         SAFE_KFREE(this->connRDMAKeyType);
         this->connRDMAKeyType = StringTk_strDup(valueStr);
      }
      else
      if(!strcmp(keyStr, "connNetFilterFile") )
      {
         SAFE_KFREE(this->connNetFilterFile);
         this->connNetFilterFile = StringTk_strDup(valueStr);
      }
      else
      if (!strcmp(keyStr, "connMaxConcurrentAttempts"))
      {
         this->connMaxConcurrentAttempts = StringTk_strToUInt(valueStr);
      }
      else
      if(!strcmp(keyStr, "connAuthFile") )
      {
         SAFE_KFREE(this->connAuthFile);
         this->connAuthFile = StringTk_strDup(valueStr);
      }
      else
      if(!strcmp(keyStr, "connDisableAuthentication") )
         this->connDisableAuthentication = StringTk_strToBool(valueStr);
      else
      if(!strcmp(keyStr, "connTcpOnlyFilterFile") )
      {
         SAFE_KFREE(this->connTcpOnlyFilterFile);
         this->connTcpOnlyFilterFile = StringTk_strDup(valueStr);
      }
      else
      if(!strcmp(keyStr, "connMessagingTimeouts"))
      {
         int cfgValCount = 3; // count value in config file in order of long, medium and short

         StrCpyList connMsgTimeoutList;
         StrCpyListIter iter;

         SAFE_KFREE(this->connMessagingTimeouts);
         this->connMessagingTimeouts = StringTk_strDup(valueStr);

         StrCpyList_init(&connMsgTimeoutList);
         StringTk_explode(valueStr, ',', &connMsgTimeoutList);

         StrCpyListIter_init(&iter, &connMsgTimeoutList);

         if (StrCpyList_length(&connMsgTimeoutList) == cfgValCount)
         {
            this->connMsgLongTimeout = StringTk_strToInt(StrCpyListIter_value(&iter)) > 0 ?
                  StringTk_strToInt(StrCpyListIter_value(&iter)) : CONN_LONG_TIMEOUT;
            StrCpyListIter_next(&iter);

            this->connMsgMediumTimeout = StringTk_strToInt(StrCpyListIter_value(&iter)) > 0 ?
                  StringTk_strToInt(StrCpyListIter_value(&iter)) : CONN_MEDIUM_TIMEOUT;
            StrCpyListIter_next(&iter);

            this->connMsgShortTimeout = StringTk_strToInt(StrCpyListIter_value(&iter)) > 0 ?
                  StringTk_strToInt(StrCpyListIter_value(&iter)) : CONN_SHORT_TIMEOUT;
         }
         else
         {
            StrCpyList_uninit(&connMsgTimeoutList);
            goto bad_config_elem;
         }
         StrCpyList_uninit(&connMsgTimeoutList);
      }
      else
      if(!strcmp(keyStr, "connRDMATimeouts"))
      {
         StrCpyList connRDMATimeoutList;
         int* cfgVals[] = {
            &this->connRDMATimeoutConnect,
            &this->connRDMATimeoutCompletion,
            &this->connRDMATimeoutFlowSend,
            &this->connRDMATimeoutFlowRecv,
            &this->connRDMATimeoutPoll
         };
         bool badVals = false;

         SAFE_KFREE(this->connRDMATimeouts);
         this->connRDMATimeouts = StringTk_strDup(valueStr);

         StrCpyList_init(&connRDMATimeoutList);
         StringTk_explode(valueStr, ',', &connRDMATimeoutList);

         if (StrCpyList_length(&connRDMATimeoutList) == sizeof(cfgVals) / sizeof(int*))
         {
            StrCpyListIter iter;
            int idx;

            StrCpyListIter_init(&iter, &connRDMATimeoutList);
            for (idx = 0; !StrCpyListIter_end(&iter); ++idx, StrCpyListIter_next(&iter))
            {
               *cfgVals[idx] = StringTk_strToInt(StrCpyListIter_value(&iter));
            }
         }
         else
         {
            badVals = true;
         }

         StrCpyList_uninit(&connRDMATimeoutList);
         if (badVals)
            goto bad_config_elem;
      }
      else
      IGNORE_CONFIG_VALUE("debugFindOtherNodes")
      IGNORE_CONFIG_VALUE("tuneNumWorkers")
      IGNORE_CONFIG_VALUE("tuneNumRetryWorkers")
      if(!strcmp(keyStr, "tunePreferredMetaFile") )
      {
         SAFE_KFREE(this->tunePreferredMetaFile);
         this->tunePreferredMetaFile = StringTk_strDup(valueStr);
      }
      else
      if(!strcmp(keyStr, "tunePreferredStorageFile") )
      {
         SAFE_KFREE(this->tunePreferredStorageFile);
         this->tunePreferredStorageFile = StringTk_strDup(valueStr);
      }
      else
      if(!strcmp(keyStr, "tuneFileCacheType") )
      {
         SAFE_KFREE(this->tuneFileCacheType);
         this->tuneFileCacheType = StringTk_strDup(valueStr);
      }
      else
      IGNORE_CONFIG_VALUE("tunePagedIOBufSize")
      IGNORE_CONFIG_VALUE("tunePagedIOBufNum")
      if(!strcmp(keyStr, "tuneFileCacheBufSize") )
         this->tuneFileCacheBufSize = StringTk_strToInt(valueStr);
      else
      if(!strcmp(keyStr, "tuneFileCacheBufNum") )
         this->tuneFileCacheBufNum = StringTk_strToInt(valueStr);
      else
      if(!strcmp(keyStr, "tunePageCacheValidityMS") )
         this->tunePageCacheValidityMS= StringTk_strToUInt(valueStr);
      else
      IGNORE_CONFIG_VALUE("tuneAttribCacheValidityMS")
      if(!strcmp(keyStr, "tuneDirSubentryCacheValidityMS") )
         this->tuneDirSubentryCacheValidityMS = StringTk_strToUInt(valueStr);
      else
      if(!strcmp(keyStr, "tuneFileSubentryCacheValidityMS") )
         this->tuneFileSubentryCacheValidityMS = StringTk_strToUInt(valueStr);
      else
      if(!strcmp(keyStr, "tuneENOENTCacheValidityMS") )
         this->tuneENOENTCacheValidityMS = StringTk_strToUInt(valueStr);
      else
      IGNORE_CONFIG_VALUE("tuneMaxWriteWorks")
      IGNORE_CONFIG_VALUE("tuneMaxReadWorks")
      IGNORE_CONFIG_VALUE("tuneAllowMultiSetWrite")
      IGNORE_CONFIG_VALUE("tuneAllowMultiSetRead")
      if(!strcmp(keyStr, "tunePathBufSize") )
         this->tunePathBufSize = StringTk_strToInt(valueStr);
      else
      if(!strcmp(keyStr, "tunePathBufNum") )
         this->tunePathBufNum = StringTk_strToInt(valueStr);
      else
      IGNORE_CONFIG_VALUE("tuneMaxReadWriteNum")
      IGNORE_CONFIG_VALUE("tuneMaxReadWriteNodesNum")
      if(!strcmp(keyStr, "tuneMsgBufSize") )
         this->tuneMsgBufSize = StringTk_strToInt(valueStr);
      else
      if(!strcmp(keyStr, "tuneMsgBufNum") )
         this->tuneMsgBufNum = StringTk_strToInt(valueStr);
      else
      if(!strcmp(keyStr, "tuneRemoteFSync") )
         this->tuneRemoteFSync = StringTk_strToBool(valueStr);
      else
      if(!strcmp(keyStr, "tuneUseGlobalFileLocks") )
         this->tuneUseGlobalFileLocks = StringTk_strToBool(valueStr);
      else
      if(!strcmp(keyStr, "tuneRefreshOnGetAttr") )
         this->tuneRefreshOnGetAttr = StringTk_strToBool(valueStr);
      else
      if(!strcmp(keyStr, "tuneInodeBlockBits") )
         this->tuneInodeBlockBits = StringTk_strToUInt(valueStr);
      else
      IGNORE_CONFIG_VALUE("tuneInodeBlockSize") // auto-generated based on tuneInodeBlockBits
      IGNORE_CONFIG_VALUE("tuneMaxClientMirrorSize") // was removed, kept here for compat
      if(!strcmp(keyStr, "tuneEarlyCloseResponse") )
         this->tuneEarlyCloseResponse = StringTk_strToBool(valueStr);
      else
      if(!strcmp(keyStr, "tuneUseGlobalAppendLocks") )
         this->tuneUseGlobalAppendLocks = StringTk_strToBool(valueStr);
      else
      if(!strcmp(keyStr, "tuneUseBufferedAppend") )
         this->tuneUseBufferedAppend = StringTk_strToBool(valueStr);
      else
      if(!strcmp(keyStr, "tuneStatFsCacheSecs") )
         this->tuneStatFsCacheSecs = StringTk_strToUInt(valueStr);
      else
      if(!strcmp(keyStr, "tuneCoherentBuffers") )
         this->tuneCoherentBuffers = StringTk_strToBool(valueStr);
      else
      if(!strcmp(keyStr, "sysMgmtdHost") )
      {
         SAFE_KFREE(this->sysMgmtdHost);
         this->sysMgmtdHost = StringTk_strDup(valueStr);
      }
      else
      if(!strcmp(keyStr, "sysInodeIDStyle") )
      {
         SAFE_KFREE(this->sysInodeIDStyle);
         this->sysInodeIDStyle = StringTk_strDup(valueStr);
      }
      else
      if(!strcmp(keyStr, "sysCreateHardlinksAsSymlinks") )
         this->sysCreateHardlinksAsSymlinks = StringTk_strToBool(valueStr);
      else
      if(!strcmp(keyStr, "sysMountSanityCheckMS") )
         this->sysMountSanityCheckMS = StringTk_strToUInt(valueStr);
      else
      if(!strcmp(keyStr, "sysSyncOnClose") )
         this->sysSyncOnClose = StringTk_strToBool(valueStr);
      else
      if(!strcmp(keyStr, "sysSessionCheckOnClose") )
         this->sysSessionCheckOnClose = StringTk_strToBool(valueStr);
      else
      if(!strcmp(keyStr, "sysSessionChecksEnabled") )
         this->sysSessionChecksEnabled = StringTk_strToBool(valueStr);
      else
      if(!strcmp(keyStr, "sysUpdateTargetStatesSecs") )
         this->sysUpdateTargetStatesSecs = StringTk_strToUInt(valueStr);
      else
      if(!strcmp(keyStr, "sysTargetOfflineTimeoutSecs") )
      {
         this->sysTargetOfflineTimeoutSecs = StringTk_strToUInt(valueStr);
         if (this->sysTargetOfflineTimeoutSecs < 30)
         {
            printk_fhgfs(KERN_WARNING, "Invalid argument for sysTargetOfflineTimeoutSecs: %s "
                  "(must be at least 30)\n", valueStr);
            return false;
         }
      }
      else
      if(!strcmp(keyStr, "sysXAttrsEnabled") )
         this->sysXAttrsEnabled = StringTk_strToBool(valueStr);
      else
      if(!strcmp(keyStr, "sysXAttrsCheckCapabilities") )
      {
         if (!strcmp(valueStr, CHECKCAPABILITIES_ALWAYS_STR))
            this->sysXAttrsCheckCapabilities = CHECKCAPABILITIES_Always;
         else if (!strcmp(valueStr, CHECKCAPABILITIES_CACHE_STR))
            this->sysXAttrsCheckCapabilities = CHECKCAPABILITIES_Cache;
         else if (!strcmp(valueStr, CHECKCAPABILITIES_NEVER_STR))
            this->sysXAttrsCheckCapabilities = CHECKCAPABILITIES_Never;
      }
      else
      if(!strcmp(keyStr, "sysACLsEnabled") )
         this->sysACLsEnabled = StringTk_strToBool(valueStr);
      else
      if (!strcmp(keyStr, "sysRenameEbusyAsXdev"))
         this->sysRenameEbusyAsXdev = StringTk_strToBool(valueStr);
      else
      if(!strcmp(keyStr, "quotaEnabled") )
         this->quotaEnabled = StringTk_strToBool(valueStr);
      else if (!strcmp(keyStr, "sysFileEventLogMask"))
      {
         if (!strcmp(valueStr, EVENTLOGMASK_NONE))
            this->eventLogMask = EventLogMask_NONE;
         else
         {
            StrCpyList parts;
            StrCpyListIter it;

            StrCpyList_init(&parts);
            StringTk_explode(valueStr, ',', &parts);

            this->eventLogMask = 0;

            StrCpyListIter_init(&it, &parts);
            while (!StrCpyListIter_end(&it))
            {
               if (!strcmp(StrCpyListIter_value(&it), EVENTLOGMASK_FLUSH))
                  this->eventLogMask |= EventLogMask_FLUSH;
               else if (!strcmp(StrCpyListIter_value(&it), EVENTLOGMASK_TRUNC))
                  this->eventLogMask |= EventLogMask_TRUNC;
               else if (!strcmp(StrCpyListIter_value(&it), EVENTLOGMASK_SETATTR))
                  this->eventLogMask |= EventLogMask_SETATTR;
               else if (!strcmp(StrCpyListIter_value(&it), EVENTLOGMASK_CLOSE))
                  this->eventLogMask |= EventLogMask_CLOSE;
               else if (!strcmp(StrCpyListIter_value(&it), EVENTLOGMASK_LINK_OP))
                  this->eventLogMask |= EventLogMask_LINK_OP;
               else if (!strcmp(StrCpyListIter_value(&it), EVENTLOGMASK_READ))
                  this->eventLogMask |= EventLogMask_READ;
               else
               {
                  StrCpyList_uninit(&parts);
                  goto bad_config_elem;
               }

               StrCpyListIter_next(&it);
            }

            StrCpyList_uninit(&parts);
         }
      }
      else if(!strcmp(keyStr, "remapConnectionFailureStatus"))
         this->remapConnectionFailureStatus = StringTk_strToUInt(valueStr);
      else if(!strcmp(keyStr, "sysNoEnterpriseFeatureMsg"))
         this->sysNoEnterpriseFeatureMsg = StringTk_strToBool(valueStr);
      else
      { // unknown element occurred
bad_config_elem:
         unknownElement = true;

         if(enableException)
         {
            printk_fhgfs(KERN_WARNING, "The config argument '%s' is invalid\n", keyStr );

            return false;
         }
      }

      // remove known elements from the map (we don't need them any longer)
      if(unknownElement)
      { // just skip the unknown element
         StrCpyMapIter_next(&iter);
      }
      else
      { // remove this element from the map
         iter = _Config_eraseFromConfigMap(this, &iter);
      }
   } // end of while loop

   return true;
}

void _Config_configMapRedefine(Config* this, char* keyStr, const char* valueStr)
{
   StrCpyMapIter iter;

   iter = StrCpyMap_find(&this->configMap, keyStr);

   if(!StrCpyMapIter_end(&iter) )
      StrCpyMap_erase(&this->configMap, keyStr);


   StrCpyMap_insert(&this->configMap, keyStr, valueStr);
}

void __Config_addLineToConfigMap(Config* this, char* line)
{
   int divPos;
   char* divPosStr;
   char* param;
   char* value;
   char* trimParam;
   char* trimValue;

   divPosStr = strchr(line, '=');

   if(!divPosStr)
   {
      char* trimCopy = StringTk_trimCopy(line);
      _Config_configMapRedefine(this, trimCopy, "");
      kfree(trimCopy);

      return;
   }

   divPos = divPosStr - line;

   param = StringTk_subStr(line, divPos);
   value = StringTk_subStr(&divPosStr[1], strlen(&divPosStr[1]) );

   trimParam = StringTk_trimCopy(param);
   trimValue = StringTk_trimCopy(value);

   _Config_configMapRedefine(this, trimParam, trimValue);

   kfree(param);
   kfree(value);

   kfree(trimParam);
   kfree(trimValue);
}

void __Config_loadFromMountConfig(Config* this, MountConfig* mountConfig)
{
   // string args

   if(mountConfig->cfgFile)
      _Config_configMapRedefine(this, "cfgFile", mountConfig->cfgFile);

   if(mountConfig->logStdFile)
      _Config_configMapRedefine(this, "logStdFile", mountConfig->logStdFile);

   if(mountConfig->sysMgmtdHost)
      _Config_configMapRedefine(this, "sysMgmtdHost", mountConfig->sysMgmtdHost);

   if(mountConfig->tunePreferredMetaFile)
      _Config_configMapRedefine(this, "tunePreferredMetaFile", mountConfig->tunePreferredMetaFile);

   if(mountConfig->tunePreferredStorageFile)
      _Config_configMapRedefine(this, "tunePreferredStorageFile",
         mountConfig->tunePreferredStorageFile);

   if(mountConfig->connInterfacesList)
   {
      char* delimiter  = mountConfig->connInterfacesList;

      for(size_t listLength = strlen(mountConfig->connInterfacesList); 
         listLength ; ++delimiter,--listLength)
      {
         if(*delimiter == ' ')
            *delimiter = ',';
      }
      _Config_configMapRedefine(this, "connInterfacesList", mountConfig->connInterfacesList);
   }

   // integer args

   if(mountConfig->logLevelDefined)
   {
      char* valueStr = StringTk_intToStr(mountConfig->logLevel);
      _Config_configMapRedefine(this, "logLevel", valueStr);
      kfree(valueStr);
   }

   if(mountConfig->connPortShiftDefined)
   {
      char* valueStr = StringTk_uintToStr(mountConfig->connPortShift);
      _Config_configMapRedefine(this, "connPortShift", valueStr);
      kfree(valueStr);
   }

   if(mountConfig->connMgmtdPortUDPDefined)
   {
      char* valueStr = StringTk_uintToStr(mountConfig->connMgmtdPortUDP);
      _Config_configMapRedefine(this, "connMgmtdPortUDP", valueStr);
      kfree(valueStr);
   }

   if(mountConfig->connMgmtdPortTCPDefined)
   {
      char* valueStr = StringTk_uintToStr(mountConfig->connMgmtdPortTCP);
      _Config_configMapRedefine(this, "connMgmtdPortTCP", valueStr);
      kfree(valueStr);
   }

   if(mountConfig->sysMountSanityCheckMSDefined)
   {
      char* valueStr = StringTk_uintToStr(mountConfig->sysMountSanityCheckMS);
      _Config_configMapRedefine(this, "sysMountSanityCheckMS", valueStr);
      kfree(valueStr);
   }

}

bool __Config_loadFromFile(struct Config* this, const char* filename)
{
   bool retVal = true;

   struct file* cfgFile;
   char* line;
   char* trimLine;
   int currentLineNum; // 1-based (just for error logging)

   //printk_fhgfs_debug(KERN_INFO, "Attempting to read config file: '%s'\n", filename);
   cfgFile = filp_open(filename, (O_RDONLY), 0);
   if(IS_ERR(cfgFile) )
   {
      printk_fhgfs(KERN_WARNING, "Failed to open config file: '%s'\n", filename);
      return false;
   }

   line = os_kmalloc(CONFIG_FILE_MAX_LINE_LENGTH);


   for(currentLineNum=1; ; currentLineNum++)
   {
      bool endOfFile;
      bool readRes;

      readRes = __Config_readLineFromFile(cfgFile, line, CONFIG_FILE_MAX_LINE_LENGTH, &endOfFile);

      // stop on end of file
      if(endOfFile)
         break;

      if(!readRes)
      { // error occurred
         printk_fhgfs(KERN_WARNING, "Error occurred while reading the config file "
            "(line: %d, file: '%s')\n", currentLineNum, filename);

         retVal = false;

         break;
      }


      // trim the line and add it to the config, ignore comment lines
      trimLine = StringTk_trimCopy(line);
      if(strlen(trimLine) && (trimLine[0] != CONFIG_FILE_COMMENT_CHAR) )
         __Config_addLineToConfigMap(this, trimLine);

      kfree(trimLine);
   }

   // clean up
   kfree(line);

   WITH_PROCESS_CONTEXT {
      filp_close(cfgFile, NULL);
   }

   return retVal;
}

/**
 * Reads each line of a file into a separate string and appends it to outList.
 *
 * All strings are trimmed and empty lines are not added to the list.
 */
bool Config_loadStringListFile(const char* filename, StrCpyList* outList)
{
   bool retVal = true;

   struct file* listFile;
   char* line;
   char* trimLine;

   //printk_fhgfs(KERN_INFO, "Attempting to read configured list file: '%s'\n", filename);
   listFile = filp_open(filename, (O_RDONLY), 0);
   if(IS_ERR(listFile) )
   {
      printk_fhgfs(KERN_WARNING, "Failed to open (config) list file: '%s'\n", filename);
      return false;
   }

   line = os_kmalloc(CONFIG_FILE_MAX_LINE_LENGTH);


   for( ; ; )
   {
      bool endOfFile;
      bool readRes;

      readRes = __Config_readLineFromFile(listFile, line, CONFIG_FILE_MAX_LINE_LENGTH,
         &endOfFile);

      // stop on end of file
      if(endOfFile)
         break;

      if(!readRes)
      { // error occurred
         printk_fhgfs(KERN_WARNING, "Error occurred while reading a (config) list file: "
            "'%s'\n", filename);

         retVal = false;

         break;
      }

      // trim the line and add it to the nodes list, ignore comment lines
      trimLine = StringTk_trimCopy(line);
      if(strlen(trimLine) && (trimLine[0] != CONFIG_FILE_COMMENT_CHAR) )
         StrCpyList_append(outList, trimLine);

      kfree(trimLine);

   }

   // clean up
   kfree(line);

   WITH_PROCESS_CONTEXT {
      filp_close(listFile, NULL);
   }

   return retVal;
}

/**
 * Wrapper for loadStringListFile() to read file into a UInt16List.
 */
bool Config_loadUInt16ListFile(struct Config* this, const char* filename, UInt16List* outList)
{
   StrCpyList strList;
   StrCpyListIter iter;
   bool loadRes;

   StrCpyList_init(&strList);

   loadRes = Config_loadStringListFile(filename, &strList);
   if(!loadRes)
      goto cleanup_and_exit;

   StrCpyListIter_init(&iter, &strList);

   for( ; !StrCpyListIter_end(&iter); StrCpyListIter_next(&iter) )
   {
      char* currentLine = StrCpyListIter_value(&iter);

      UInt16List_append(outList, StringTk_strToUInt(currentLine) );
   }

cleanup_and_exit:
   StrCpyList_uninit(&strList);

   return loadRes;
}

/**
 * Note: Cuts off lines that are longer than the buffer
 *
 * @param outEndOfFile true if end of file reached
 * @return false on file io error
 */
bool __Config_readLineFromFile(struct file* cfgFile,
   char* buf, size_t bufLen, bool* outEndOfFile)
{
   size_t numRead;
   bool endOfLine;
   bool erroroccurred;

   *outEndOfFile = false;
   endOfLine = false;
   erroroccurred = false;


   for(numRead = 0; numRead < (bufLen-1); numRead++)
   {
      char charBuf;

      ssize_t readRes = Config_fs_read(cfgFile, &charBuf, 1, &cfgFile->f_pos);

      if( (readRes > 0) && (charBuf == '\n') )
      { // end of line
         endOfLine = true;
         break;
      }
      else
      if(readRes > 0)
      { // any normal character
         buf[numRead] = charBuf;
      }
      else
      if(readRes == 0)
      { // end of file
         *outEndOfFile = true;
         break;
      }
      else
      { // error occurred
         printk_fhgfs(KERN_WARNING, "Failed to read from file at line position: %lld\n",
            (long long)numRead);

         erroroccurred = true;
         break;
      }

   }

   buf[numRead] = 0; // add terminating zero

   // read the rest of the line if it is longer than the buffer
   while(!endOfLine && !(*outEndOfFile) && !erroroccurred)
   {
      char charBuf;

      ssize_t readRes = Config_fs_read(cfgFile, &charBuf, 1, &cfgFile->f_pos);
      if( (readRes > 0) && (charBuf == '\n') )
         endOfLine = true;
      if(readRes == 0)
         *outEndOfFile = true;
      else
      if(readRes < 0)
         erroroccurred = true;
   }



   return !erroroccurred;

}

/*
 * Set val to defVal if val == condVal.
 * return true if val was assigned
 */
static bool __Config_setIfEqualInt(int* val, int condVal, int defVal)
{
   if (*val == condVal)
   {
      *val = defVal;
      return true;
   }
   return false;
}

/*
 * Ensure val is at least minVal.
 */
static void __Config_ensureMinInt(int* val, int minVal, const char* name)
{
   if (*val < minVal)
   {
      *val = minVal;
      printk_fhgfs(KERN_WARNING, "%s is too low, setting to %d\n", name, minVal);
   }
}

/**
 * Init values that are not directly set by the user, but computed from values that the user
 * has set.
 */
bool __Config_initImplicitVals(Config* this)
{
   __Config_initConnNumCommRetries(this);
   __Config_initTuneFileCacheTypeNum(this);
   __Config_initSysInodeIDStyleNum(this);
   __Config_initLogTypeNum(this);
   __Config_initConnRDMAKeyTypeNum(this);

   // tuneMsgBufNum
   if(!this->tuneMsgBufNum)
      this->tuneMsgBufNum = (num_online_cpus() * 4) + 1;

   // tuneFileCacheBufNum
   if(!this->tuneFileCacheBufNum)
      this->tuneFileCacheBufNum = MAX(num_online_cpus() * 4, 4);

   // tuneInodeBlockSize
   this->tuneInodeBlockSize = (1 << this->tuneInodeBlockBits);

   /* tuneUseBufferedAppend: allow only in combination with global locks
      (locally locked code paths are currently not prepared to work with buffering) */
   if(!this->tuneUseGlobalAppendLocks)
      this->tuneUseBufferedAppend = false;

   __Config_ensureMinInt(&this->connRDMABufNum, CONFIG_CONN_RDMA_BUFNUM_MIN, "connRDMABufNum");
   __Config_ensureMinInt(&this->connRDMABufSize, PAGE_SIZE, "connRDMABufSize");
   if (!__Config_setIfEqualInt(&this->connRDMAFragmentSize, 0, this->connRDMABufSize))
      __Config_ensureMinInt(&this->connRDMAFragmentSize, PAGE_SIZE, "connRDMAFragmentSize");

   if (!__Config_setIfEqualInt(&this->connRDMAMetaBufNum, CONFIG_CONN_RDMA_DEFAULT, this->connRDMABufNum))
      __Config_ensureMinInt(&this->connRDMAMetaBufNum, CONFIG_CONN_RDMA_BUFNUM_MIN, "connRDMAMetaBufNum");

   if (!__Config_setIfEqualInt(&this->connRDMAMetaBufSize, CONFIG_CONN_RDMA_DEFAULT, this->connRDMABufSize))
      __Config_ensureMinInt(&this->connRDMAMetaBufSize, PAGE_SIZE, "connRDMAMetaBufSize");
   
   if (!__Config_setIfEqualInt(&this->connRDMAMetaFragmentSize, 0, this->connRDMAMetaBufSize))
      if (!__Config_setIfEqualInt(&this->connRDMAMetaFragmentSize, CONFIG_CONN_RDMA_DEFAULT, this->connRDMAFragmentSize))
         __Config_ensureMinInt(&this->connRDMAMetaFragmentSize, PAGE_SIZE, "connRDMAMetaFragmentSize");

   if (this->connTCPRcvBufSize == 0)
   {
      /* 0 indicates that legacy behavior should be preserved. Legacy behavior used RDMA
         settings for TCP bufsize. */
      this->connTCPRcvBufSize = this->connRDMABufNum * this->connRDMABufSize;
   }

   if (this->connUDPRcvBufSize == 0)
   {
      /* 0 indicates that legacy behavior should be preserved. Legacy behavior used RDMA
         settings for UDP bufsize. */
      this->connUDPRcvBufSize = this->connRDMABufNum * this->connRDMABufSize;
   }

   // Automatically enable XAttrs if ACLs have been enabled
   if (this->sysACLsEnabled && !this->sysXAttrsEnabled)
   {
      this->sysXAttrsEnabled = true;
      this->sysXAttrsImplicitlyEnabled = true;
   }
   else
   {
      this->sysXAttrsImplicitlyEnabled = false;
   }

   // connAuthHash
   return __Config_initConnAuthHash(this, this->connAuthFile, &this->connAuthHash);
}

void __Config_initConnNumCommRetries(Config* this)
{
   // note: keep in sync with MessagingTk_getRetryWaitMS()

   unsigned retrySecs = this->connCommRetrySecs;
   unsigned retryMS = retrySecs * 1000;

   unsigned numRetries;

   if(retrySecs <= 60)
   { // 12 x 5sec retries during 1st min
      numRetries = (retryMS + (5000-1) ) / 5000;
   }
   else
   if(retrySecs <= 300)
   { // 12 x 20sec retries during 2nd to 5th min
      numRetries = (retryMS + (20000-1) - 60000) / 20000; // without 1st min
      numRetries += 12; // add 1st min
   }
   else
   { // 60 sec retries after 5th min
      numRetries = (retryMS + (60000-1) - (60000*5) ) / 60000; // without first 5 mins
      numRetries += 12; // add 1st min
      numRetries += 12; // add 2nd to 5th min
   }

   this->connNumCommRetries = numRetries;
}

void __Config_initTuneFileCacheTypeNum(Config* this)
{
   const char* valueStr = this->tuneFileCacheType;

   if(!strcasecmp(valueStr, FILECACHETYPE_NATIVE_STR))
      this->tuneFileCacheTypeNum = FILECACHETYPE_Native;
   else
   if(!strcasecmp(valueStr, FILECACHETYPE_BUFFERED_STR))
      this->tuneFileCacheTypeNum = FILECACHETYPE_Buffered;
   else
   if(!strcasecmp(valueStr, FILECACHETYPE_PAGED_STR))
      this->tuneFileCacheTypeNum = FILECACHETYPE_Paged;
   else
      this->tuneFileCacheTypeNum = FILECACHETYPE_None;
}

const char* Config_fileCacheTypeNumToStr(FileCacheType cacheType)
{
   switch(cacheType)
   {
      case FILECACHETYPE_Native:
         return FILECACHETYPE_NATIVE_STR;
      case FILECACHETYPE_Buffered:
         return FILECACHETYPE_BUFFERED_STR;
      case FILECACHETYPE_Paged:
         return FILECACHETYPE_PAGED_STR;

      default:
         return FILECACHETYPE_NONE_STR;
   }
}

void __Config_initSysInodeIDStyleNum(Config* this)
{
   const char* valueStr = this->sysInodeIDStyle;

   if(!strcasecmp(valueStr, INODEIDSTYLE_HASH64HSIEH_STR))
      this->sysInodeIDStyleNum = INODEIDSTYLE_Hash64HSieh;
   else
   if(!strcasecmp(valueStr, INODEIDSTYLE_HASH32HSIEH_STR))
      this->sysInodeIDStyleNum = INODEIDSTYLE_Hash32Hsieh;
   else
   if(!strcasecmp(valueStr, INODEIDSTYLE_HASH64MD4_STR))
      this->sysInodeIDStyleNum = INODEIDSTYLE_Hash64MD4;
   else
   if(!strcasecmp(valueStr, INODEIDSTYLE_HASH32MD4_STR))
      this->sysInodeIDStyleNum = INODEIDSTYLE_Hash32MD4;
   else
   { // default
      printk_fhgfs(KERN_INFO, "Unknown Inode-Hash: %s. Defaulting to hash64md4.\n", valueStr);
      this->sysInodeIDStyleNum = INODEIDSTYLE_Default;
   }
}

const char* Config_inodeIDStyleNumToStr(InodeIDStyle inodeIDStyle)
{
   switch(inodeIDStyle)
   {
      case INODEIDSTYLE_Hash64HSieh:
         return INODEIDSTYLE_HASH64HSIEH_STR;

      case INODEIDSTYLE_Hash32MD4:
         return INODEIDSTYLE_HASH32MD4_STR;

      case INODEIDSTYLE_Hash64MD4:
         return INODEIDSTYLE_HASH64MD4_STR;

      default:
         return INODEIDSTYLE_HASH32HSIEH_STR;
   }
}

void __Config_initLogTypeNum(Config* this)
{
   const char* valueStr = this->logType;

   if(!strcasecmp(valueStr, LOGGERTYPE_SYSLOG_STR))
      this->logTypeNum = LOGTYPE_Syslog;
   else
      this->logTypeNum = LOGTYPE_Helperd;
}

const char* Config_logTypeNumToStr(LogType logType)
{
   switch(logType)
   {
      case LOGTYPE_Syslog:
         return LOGGERTYPE_SYSLOG_STR;

      default:
         return LOGGERTYPE_HELPERD_STR;
   }
}

const char* Config_eventLogMaskToStr(enum EventLogMask mask)
{
#define ELM_PART_FLUSH(Prefix) \
   (mask & EventLogMask_FLUSH \
    ? Prefix "," EVENTLOGMASK_FLUSH \
    : Prefix)
#define ELM_PART_TRUNC(Prefix) \
   (mask & EventLogMask_TRUNC \
    ? ELM_PART_FLUSH(Prefix "," EVENTLOGMASK_TRUNC) \
    : ELM_PART_FLUSH(Prefix))
#define ELM_PART_SETATTR(Prefix) \
   (mask & EventLogMask_SETATTR \
    ? ELM_PART_TRUNC(Prefix "," EVENTLOGMASK_SETATTR) \
    : ELM_PART_TRUNC(Prefix))
#define ELM_PART_CLOSE(Prefix) \
   (mask & EventLogMask_CLOSE \
    ? ELM_PART_SETATTR(Prefix "," EVENTLOGMASK_CLOSE) \
    : ELM_PART_SETATTR(Prefix))
#define ELM_PART_LINK_OP(Prefix) \
   (mask & EventLogMask_LINK_OP \
    ? ELM_PART_CLOSE(Prefix "," EVENTLOGMASK_LINK_OP) \
    : ELM_PART_CLOSE(Prefix))
#define ELM_PART_READ(Prefix) \
   (mask & EventLogMask_READ \
    ? ELM_PART_LINK_OP(Prefix "," EVENTLOGMASK_READ) \
    : ELM_PART_LINK_OP(Prefix))
   // If new event types are added here, the return below must be updated.

   if (mask == EventLogMask_NONE)
      return EVENTLOGMASK_NONE;
   else
      return ELM_PART_READ("") + 1;
}

void __Config_initConnRDMAKeyTypeNum(Config* this)
{
   const char* valueStr = this->connRDMAKeyType;

   if(!strcasecmp(valueStr, RDMAKEYTYPE_UNSAFE_DMA_STR))
      this->connRDMAKeyTypeNum = RDMAKEYTYPE_UnsafeDMA;
   else if(!strcasecmp(valueStr, RDMAKEYTYPE_REGISTER_STR))
      this->connRDMAKeyTypeNum = RDMAKEYTYPE_Register;
   else
      this->connRDMAKeyTypeNum = RDMAKEYTYPE_UnsafeGlobal;
}

const char* Config_rdmaKeyTypeNumToStr(RDMAKeyType keyType)
{
   switch(keyType)
   {
      case RDMAKEYTYPE_Register:
         return RDMAKEYTYPE_REGISTER_STR;
      case RDMAKEYTYPE_UnsafeDMA:
         return RDMAKEYTYPE_UNSAFE_DMA_STR;
      default:
         return RDMAKEYTYPE_UNSAFE_GLOBAL_STR;
   }
}

const char* Config_checkCapabilitiesTypeToStr(CheckCapabilities checkCapabilities)
{
   switch(checkCapabilities)
   {
      case CHECKCAPABILITIES_Cache:
         return CHECKCAPABILITIES_CACHE_STR;
      case CHECKCAPABILITIES_Never:
         return CHECKCAPABILITIES_NEVER_STR;
      default:
         return CHECKCAPABILITIES_ALWAYS_STR;
   }
}

/**
 * Generate connection authentication hash based on contents of given authentication file.
 *
 * @param outConnAuthHash will be set to 0 if file is not defined
 * @return true on success or unset file, false on error
 */
bool __Config_initConnAuthHash(Config* this, char* connAuthFile, uint64_t* outConnAuthHash)
{
   struct file* fileHandle;
   char* buf;
   ssize_t readRes;


   if(!connAuthFile || !StringTk_hasLength(connAuthFile))
   {
      if (this->connDisableAuthentication)
      {
         *outConnAuthHash = 0;
         return true; // connAuthFile explicitly disabled => no hash to be generated
      }
      else
      {
         printk_fhgfs(KERN_WARNING, "No connAuthFile configured. Using BeeGFS without connection authentication is considered insecure and is not recommended. If you really want or need to run BeeGFS without connection authentication, please set connDisableAuthentication to true.");
         return false;
      }
   }


   // open file...

   fileHandle = filp_open(connAuthFile, O_RDONLY, 0);
   if(IS_ERR(fileHandle) )
   {
      printk_fhgfs(KERN_WARNING, "Failed to open auth file: '%s'\n", connAuthFile);
      return false;
   }

   // read file...

   buf = os_kmalloc(CONFIG_AUTHFILE_READSIZE);
   if(unlikely(!buf) )
   {
      printk_fhgfs(KERN_WARNING, "Failed to alloc mem for auth file reading: '%s'\n", connAuthFile);

      WITH_PROCESS_CONTEXT {
         filp_close(fileHandle, NULL);
      }

      return false;
   }

   readRes = Config_fs_read(fileHandle, buf, CONFIG_AUTHFILE_READSIZE, &fileHandle->f_pos);

   WITH_PROCESS_CONTEXT {
      filp_close(fileHandle, NULL);
   }


   if(readRes < 0)
   {
      printk_fhgfs(KERN_WARNING, "Unable to read auth file: '%s'\n", connAuthFile);
      return false;
   }

   // empty authFile is probably unintended, so treat it as error
   if(!readRes || (readRes < CONFIG_AUTHFILE_MINSIZE) )
   {
      printk_fhgfs(KERN_WARNING, "Auth file is empty or too small: '%s'\n", connAuthFile);
      return false;
   }


   { // hash file contents

      // (hsieh hash only generates 32bit hashes, so we make two hashes for 64 bits)

      int len1stHalf = readRes / 2;
      int len2ndHalf = readRes - len1stHalf;

      uint64_t high = HashTk_hash32(HASHTK_HSIEHHASH32, buf, len1stHalf);
      uint64_t low  = HashTk_hash32(HASHTK_HSIEHHASH32, &buf[len1stHalf], len2ndHalf);

      *outConnAuthHash = (high << 32) | low;
   }

   // clean up
   kfree(buf);


   return true;
}

