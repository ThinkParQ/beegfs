#ifndef CONFIG_H_
#define CONFIG_H_

#include <common/Common.h>
#include <common/toolkit/list/StrCpyListIter.h>
#include <common/toolkit/list/UInt16ListIter.h>
#include <common/toolkit/tree/StrCpyMap.h>
#include <common/toolkit/tree/StrCpyMapIter.h>
#include <common/toolkit/StringTk.h>
#include <app/config/MountConfig.h>


struct Config;
typedef struct Config Config;

enum FileCacheType;
typedef enum FileCacheType FileCacheType;

enum InodeIDStyle;
typedef enum InodeIDStyle InodeIDStyle;

enum LogType;
typedef enum LogType LogType;


extern __must_check bool Config_init(Config* this, MountConfig* mountConfig);
extern Config* Config_construct(MountConfig* mountConfig);
extern void Config_uninit(Config* this);
extern void Config_destruct(Config* this);

extern bool _Config_initConfig(Config* this, MountConfig* mountConfig,
   bool enableException);
extern StrCpyMapIter _Config_eraseFromConfigMap(Config* this, StrCpyMapIter* iter);
extern void _Config_loadDefaults(Config* this);
extern bool _Config_applyConfigMap(Config* this, bool enableException);
extern void _Config_configMapRedefine(Config* this, char* keyStr, const char* valueStr);
extern void __Config_addLineToConfigMap(Config* this, char* line);

extern void __Config_loadFromMountConfig(Config* this, MountConfig* mountConfig);
extern bool __Config_loadFromFile(struct Config* this, const char* filename);
extern bool Config_loadStringListFile(const char* filename,
   StrCpyList* outList);
extern bool Config_loadUInt16ListFile(struct Config* this, const char* filename,
   UInt16List* outList);
extern const char* __Config_createDefaultCfgFilename(void);

extern bool __Config_initImplicitVals(Config* this);
extern void __Config_initConnNumCommRetries(Config* this);
extern void __Config_initTuneFileCacheTypeNum(Config* this);
void __Config_initSysInodeIDStyleNum(Config* this);
void __Config_initLogTypeNum(Config* this);
bool __Config_initConnAuthHash(Config* this, char* connAuthFile, uint64_t* outConnAuthHash);

// conversion
const char* Config_fileCacheTypeNumToStr(FileCacheType cacheType);
const char* Config_inodeIDStyleNumToStr(InodeIDStyle inodeIDStyle);
const char* Config_logTypeNumToStr(LogType logType);

// getters & setters
static inline char* Config_getCfgFile(Config* this);
static inline int Config_getLogLevel(Config* this);
static inline LogType Config_getLogTypeNum(Config* this);
static inline void Config_setLogTypeNum(Config* this, LogType logType);
static inline bool Config_getLogClientID(Config* this);
static inline char* Config_getLogHelperdIP(Config* this);
static inline bool Config_getConnUseSDP(Config* this);
static inline bool Config_getConnUseRDMA(Config* this);
static inline int Config_getConnClientPortUDP(Config* this);
static inline int Config_getConnMetaPortUDP(Config* this);
static inline int Config_getConnStoragePortUDP(Config* this);
static inline int Config_getConnMgmtdPortUDP(Config* this);
static inline int Config_getConnMetaPortTCP(Config* this);
static inline int Config_getConnStoragePortTCP(Config* this);
static inline int Config_getConnHelperdPortTCP(Config* this);
static inline int Config_getConnMgmtdPortTCP(Config* this);
static inline unsigned Config_getConnMaxInternodeNum(Config* this);
static inline char* Config_getConnInterfacesFile(Config* this);
static inline unsigned Config_getConnFallbackExpirationSecs(Config* this);
static inline unsigned Config_getConnNumCommRetries(Config* this);
static inline unsigned Config_getConnCommRetrySecs(Config* this);
static inline bool Config_getConnUnmountRetries(Config* this);
static inline unsigned Config_getConnRDMABufSize(Config* this);
static inline unsigned Config_getConnRDMABufNum(Config* this);
static inline int Config_getConnRDMATypeOfService(Config* this);
static inline char* Config_getConnNetFilterFile(Config* this);
static inline char* Config_getConnAuthFile(Config* this);
static inline uint64_t Config_getConnAuthHash(Config* this);
static inline unsigned Config_getConnRecvNonIntrTimeoutMS(Config* this);
static inline char* Config_getConnTcpOnlyFilterFile(Config* this);
static inline char* Config_getTunePreferredMetaFile(Config* this);
static inline char* Config_getTunePreferredStorageFile(Config* this);
static inline char* Config_getTuneFileCacheType(Config* this);
static inline FileCacheType Config_getTuneFileCacheTypeNum(Config* this);
static inline int Config_getTuneFileCacheBufSize(Config* this);
static inline int Config_getTuneFileCacheBufNum(Config* this);
static inline int Config_getTunePathBufSize(Config* this);
static inline int Config_getTunePathBufNum(Config* this);
static inline int Config_getTuneMsgBufSize(Config* this);
static inline int Config_getTuneMsgBufNum(Config* this);
static inline unsigned Config_getTunePageCacheValidityMS(Config* this);
static inline unsigned Config_getTuneAttribCacheValidityMS(Config* this);
static inline unsigned Config_getTuneDirSubentryCacheValidityMS(Config* this);
static inline unsigned Config_getTuneFileSubentryCacheValidityMS(Config* this);
static inline bool Config_getTuneRemoteFSync(Config* this);
static inline bool Config_getTuneUseGlobalFileLocks(Config* this);
static inline bool Config_getTuneRefreshOnGetAttr(Config* this);
static inline void Config_setTuneRefreshOnGetAttr(Config* this);
static inline unsigned Config_getTuneInodeBlockBits(Config* this);
static inline unsigned Config_getTuneInodeBlockSize(Config* this);
static inline bool Config_getTuneEarlyCloseResponse(Config* this);
static inline bool Config_getTuneUseGlobalAppendLocks(Config* this);
static inline bool Config_getTuneUseBufferedAppend(Config* this);
static inline unsigned Config_getTuneStatFsCacheSecs(Config* this);
static inline bool Config_getTuneCoherentBuffers(Config* this);

static inline char* Config_getSysMgmtdHost(Config* this);
static inline char* Config_getSysInodeIDStyle(Config* this);
static inline InodeIDStyle Config_getSysInodeIDStyleNum(Config* this);
static inline bool Config_getSysCreateHardlinksAsSymlinks(Config* this);
static inline unsigned Config_getSysMountSanityCheckMS(Config* this);
static inline bool Config_getSysSyncOnClose(Config* this);
static inline bool Config_getSysSessionCheckOnClose(Config* this);
static inline unsigned Config_getSysUpdateTargetStatesSecs(Config* this);
static inline unsigned Config_getSysTargetOfflineTimeoutSecs(Config* this);
static inline bool Config_getSysXAttrsEnabled(Config* this);
static inline bool Config_getSysACLsEnabled(Config* this);
static inline bool Config_getSysXAttrsImplicitlyEnabled(Config* this);

static inline bool Config_getQuotaEnabled(Config* this);


enum FileCacheType
{ FILECACHETYPE_None = 0, FILECACHETYPE_Buffered = 1, FILECACHETYPE_Paged = 2,
  FILECACHETYPE_Native = 3};


#define INODEIDSTYLE_HASH32HSIEH_STR   "hash32"
#define INODEIDSTYLE_HASH64HSIEH_STR   "hash64"
#define INODEIDSTYLE_HASH32MD4_STR     "md4hash32"
#define INODEIDSTYLE_HASH64MD4_STR     "md4hash64"
#define INODEIDSTYLE_DEFAULT           INODEIDSTYLE_HASH64MD4_STR

enum InodeIDStyle
{
   INODEIDSTYLE_Hash32Hsieh     = 0,    // hsieh32
   INODEIDSTYLE_Hash32MD4,              // half-md4
   INODEIDSTYLE_Hash64HSieh,            // hsieh32
   INODEIDSTYLE_Hash64MD4               // half-md4
};
#define INODEIDSTYLE_Default  INODEIDSTYLE_Hash64MD4


enum LogType
   {LOGTYPE_Helperd = 0, LOGTYPE_Syslog = 1};

struct Config
{
   // configurables
   char*          cfgFile;

   int            logLevel;
   bool     logClientID;
   char*          logHelperdIP;
   char*          logType; // logger type (helperd, syslog)
   LogType        logTypeNum; // auto-generated based on logType string

   int            connPortShift; // shifts all UDP and TCP ports
   int            connClientPortUDP;
   int            connMetaPortUDP;
   int            connStoragePortUDP;
   int            connMgmtdPortUDP;
   int            connMetaPortTCP;
   int            connStoragePortTCP;
   int            connHelperdPortTCP;
   int            connMgmtdPortTCP;
   bool     connUseSDP;
   bool     connUseRDMA;
   unsigned       connMaxInternodeNum;
   char*          connInterfacesFile;
   unsigned       connFallbackExpirationSecs;
   unsigned       connNumCommRetries; // auto-computed from connCommRetrySecs
   unsigned       connCommRetrySecs;
   bool     connUnmountRetries;
   unsigned       connRDMABufSize;
   unsigned       connRDMABufNum;
   int            connRDMATypeOfService;
   char*          connNetFilterFile; // allowed IP addresses (all IPs allowed, if empty)
   char*          connAuthFile;
   uint64_t       connAuthHash; // implicitly set based on hash of connAuthFile contents
   unsigned       connRecvNonIntrTimeoutMS; // timeout before allowing interuptions, e.g. SIGINT
   char*          connTcpOnlyFilterFile; // allow only plain TCP (no RDMA etc) to these IPs

   char*          tunePreferredMetaFile;
   char*          tunePreferredStorageFile;
   char*          tuneFileCacheType;
   FileCacheType  tuneFileCacheTypeNum; // auto-generated based on tuneFileCacheType
   int            tuneFileCacheBufSize;
   int            tuneFileCacheBufNum; // 0 means automatic setting
   unsigned       tunePageCacheValidityMS;
   unsigned       tuneAttribCacheValidityMS;
   unsigned       tuneDirSubentryCacheValidityMS;
   unsigned       tuneFileSubentryCacheValidityMS;
   int            tunePathBufSize;
   int            tunePathBufNum;
   int            tuneMsgBufSize;
   int            tuneMsgBufNum; // 0 means automatic setting
   bool     tuneRemoteFSync;
   bool     tuneUseGlobalFileLocks; // false means local flock/fcntl locks
   bool     tuneRefreshOnGetAttr; // false means don't refresh on getattr
   unsigned       tuneInodeBlockBits; // bitshift for optimal io size seen by stat() (2^n)
   unsigned       tuneInodeBlockSize; // auto-generated based on tuneInodeBlockBits
   bool     tuneEarlyCloseResponse; // don't wait for chunk files close result
   bool     tuneUseGlobalAppendLocks; // false means local append locks
   bool     tuneUseBufferedAppend; // false disables buffering of append writes
   unsigned       tuneStatFsCacheSecs; // 0 disables caching of free space info from servers
   bool           tuneCoherentBuffers; // try to keep buffer cache and page cache coherent

   char*          sysMgmtdHost;
   char*          sysInodeIDStyle;
   InodeIDStyle   sysInodeIDStyleNum; // auto-generated based on sysInodeIDStyle
   bool     sysCreateHardlinksAsSymlinks;
   unsigned       sysMountSanityCheckMS;
   bool     sysSyncOnClose;
   bool     sysSessionCheckOnClose;
   unsigned       sysUpdateTargetStatesSecs;
   unsigned       sysTargetOfflineTimeoutSecs;

   bool     sysXAttrsEnabled;
   bool     sysACLsEnabled;
   bool     sysXAttrsImplicitlyEnabled; // True when XAttrs have not been enabled in the config file
                                        // but have been enabled by __Config_initImplicitVals
                                        // because ACLs are enabled in the config and XAs are needed
                                        // to store the ACLs.

   bool     quotaEnabled;

   /* workaround for rename of closed files on nfs */
   bool sysRenameEbusyAsXdev;


   // internals
   StrCpyMap configMap;
};

char* Config_getCfgFile(Config* this)
{
   return this->cfgFile;
}

int Config_getLogLevel(Config* this)
{
   return this->logLevel;
}

LogType Config_getLogTypeNum(Config* this)
{
   return this->logTypeNum;
}

void Config_setLogTypeNum(Config* this, LogType logType)
{
   this->logTypeNum = logType;
}

bool Config_getLogClientID(Config* this)
{
   return this->logClientID;
}

char* Config_getLogHelperdIP(Config* this)
{
   return this->logHelperdIP;
}


int Config_getConnClientPortUDP(Config* this)
{
   return this->connClientPortUDP ? (this->connClientPortUDP + this->connPortShift) : 0;
}

int Config_getConnMetaPortUDP(Config* this)
{
   return this->connMetaPortUDP ? (this->connMetaPortUDP + this->connPortShift) : 0;
}

int Config_getConnStoragePortUDP(Config* this)
{
   return this->connStoragePortUDP ? (this->connStoragePortUDP + this->connPortShift) : 0;
}

int Config_getConnMgmtdPortUDP(Config* this)
{
   return this->connMgmtdPortUDP ? (this->connMgmtdPortUDP + this->connPortShift) : 0;
}

int Config_getConnMetaPortTCP(Config* this)
{
   return this->connMetaPortTCP ? (this->connMetaPortTCP + this->connPortShift) : 0;
}

int Config_getConnStoragePortTCP(Config* this)
{
   return this->connStoragePortTCP ? (this->connStoragePortTCP + this->connPortShift) : 0;
}

int Config_getConnHelperdPortTCP(Config* this)
{
   return this->connHelperdPortTCP ? (this->connHelperdPortTCP + this->connPortShift) : 0;
}

int Config_getConnMgmtdPortTCP(Config* this)
{
   return this->connMgmtdPortTCP ? (this->connMgmtdPortTCP + this->connPortShift) : 0;
}

bool Config_getConnUseSDP(Config* this)
{
   return this->connUseSDP;
}

bool Config_getConnUseRDMA(Config* this)
{
   return this->connUseRDMA;
}

unsigned Config_getConnMaxInternodeNum(Config* this)
{
   return this->connMaxInternodeNum;
}

char* Config_getConnInterfacesFile(Config* this)
{
   return this->connInterfacesFile;
}

unsigned Config_getConnFallbackExpirationSecs(Config* this)
{
   return this->connFallbackExpirationSecs;
}

unsigned Config_getConnNumCommRetries(Config* this)
{
   return this->connNumCommRetries;
}

unsigned Config_getConnCommRetrySecs(Config* this)
{
   return this->connCommRetrySecs;
}

bool Config_getConnUnmountRetries(Config* this)
{
   return this->connUnmountRetries;
}

unsigned Config_getConnRDMABufSize(Config* this)
{
   return this->connRDMABufSize;
}

unsigned Config_getConnRDMABufNum(Config* this)
{
   return this->connRDMABufNum;
}

int Config_getConnRDMATypeOfService(Config* this)
{
   return this->connRDMATypeOfService;
}

char* Config_getConnNetFilterFile(Config* this)
{
   return this->connNetFilterFile;
}

char* Config_getConnAuthFile(Config* this)
{
   return this->connAuthFile;
}

uint64_t Config_getConnAuthHash(Config* this)
{
   return this->connAuthHash;
}

unsigned Config_getConnRecvNonIntrTimeoutMS(Config* this)
{
   return this->connRecvNonIntrTimeoutMS;
}

char* Config_getConnTcpOnlyFilterFile(Config* this)
{
   return this->connTcpOnlyFilterFile;
}

char* Config_getTunePreferredMetaFile(Config* this)
{
   return this->tunePreferredMetaFile;
}

char* Config_getTunePreferredStorageFile(Config* this)
{
   return this->tunePreferredStorageFile;
}

char* Config_getTuneFileCacheType(Config* this)
{
   return this->tuneFileCacheType;
}

FileCacheType Config_getTuneFileCacheTypeNum(Config* this)
{
   return this->tuneFileCacheTypeNum;
}

int Config_getTuneFileCacheBufSize(Config* this)
{
   return this->tuneFileCacheBufSize;
}

int Config_getTuneFileCacheBufNum(Config* this)
{
   return this->tuneFileCacheBufNum;
}

int Config_getTunePathBufSize(Config* this)
{
   return this->tunePathBufSize;
}

int Config_getTunePathBufNum(Config* this)
{
   return this->tunePathBufNum;
}

int Config_getTuneMsgBufSize(Config* this)
{
   return this->tuneMsgBufSize;
}

int Config_getTuneMsgBufNum(Config* this)
{
   return this->tuneMsgBufNum;
}

unsigned Config_getTunePageCacheValidityMS(Config* this)
{
   return this->tunePageCacheValidityMS;
}

unsigned Config_getTuneAttribCacheValidityMS(Config* this)
{
   return this->tuneAttribCacheValidityMS;
}

unsigned Config_getTuneDirSubentryCacheValidityMS(Config* this)
{
   return this->tuneDirSubentryCacheValidityMS;
}

unsigned Config_getTuneFileSubentryCacheValidityMS(Config* this)
{
   return this->tuneFileSubentryCacheValidityMS;
}

bool Config_getTuneRemoteFSync(Config* this)
{
   return this->tuneRemoteFSync;
}

bool Config_getTuneUseGlobalFileLocks(Config* this)
{
   return this->tuneUseGlobalFileLocks;
}

bool Config_getTuneRefreshOnGetAttr(Config* this)
{
   return this->tuneRefreshOnGetAttr;
}

bool Config_getTuneCoherentBuffers(Config* this)
{
   return this->tuneCoherentBuffers;
}

/**
 * Special function to automatically enable TuneRefreshOnGetAttr, e.g. for NFS exports.
 *
 * Note: We do not use any locks here assuming the right value will propate to all cores rather
 *       soon.
 */
void Config_setTuneRefreshOnGetAttr(Config* this)
{
   this->tuneRefreshOnGetAttr = true;

   // do a memory barrier, so that other CPUs get the new value as soon as possible
   smp_wmb();
}


unsigned Config_getTuneInodeBlockBits(Config* this)
{
   return this->tuneInodeBlockBits;
}

unsigned Config_getTuneInodeBlockSize(Config* this)
{
   return this->tuneInodeBlockSize;
}

bool Config_getTuneEarlyCloseResponse(Config* this)
{
   return this->tuneEarlyCloseResponse;
}

bool Config_getTuneUseGlobalAppendLocks(Config* this)
{
   return this->tuneUseGlobalAppendLocks;
}

bool Config_getTuneUseBufferedAppend(Config* this)
{
   return this->tuneUseBufferedAppend;
}

unsigned Config_getTuneStatFsCacheSecs(Config* this)
{
   return this->tuneStatFsCacheSecs;
}

char* Config_getSysMgmtdHost(Config* this)
{
   return this->sysMgmtdHost;
}

char* Config_getSysInodeIDStyle(Config* this)
{
   return this->sysInodeIDStyle;
}

InodeIDStyle Config_getSysInodeIDStyleNum(Config* this)
{
   return this->sysInodeIDStyleNum;
}

bool Config_getSysCreateHardlinksAsSymlinks(Config* this)
{
   return this->sysCreateHardlinksAsSymlinks;
}

unsigned Config_getSysMountSanityCheckMS(Config* this)
{
   return this->sysMountSanityCheckMS;
}

bool Config_getSysSyncOnClose(Config* this)
{
   return this->sysSyncOnClose;
}

bool Config_getSysSessionCheckOnClose(Config* this)
{
   return this->sysSessionCheckOnClose;
}

unsigned Config_getSysUpdateTargetStatesSecs(Config* this)
{
   return this->sysUpdateTargetStatesSecs;
}

unsigned Config_getSysTargetOfflineTimeoutSecs(Config* this)
{
   return this->sysTargetOfflineTimeoutSecs;
}

bool Config_getSysXAttrsEnabled(Config* this)
{
   return this->sysXAttrsEnabled;
}

bool Config_getSysACLsEnabled(Config* this)
{
   return this->sysACLsEnabled;
}

bool Config_getSysXAttrsImplicitlyEnabled(Config* this)
{
   return this->sysXAttrsImplicitlyEnabled;
}

bool Config_getQuotaEnabled(Config* this)
{
   return this->quotaEnabled;
}

#endif /*CONFIG_H_*/
