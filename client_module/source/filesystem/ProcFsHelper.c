#include <filesystem/ProcFsHelper.h>
#include <app/config/Config.h>
#include <app/log/Logger.h>
#include <common/toolkit/list/UInt16List.h>
#include <common/toolkit/list/UInt8List.h>
#include <common/toolkit/NodesTk.h>
#include <common/toolkit/StringTk.h>
#include <common/nodes/TargetStateStore.h>
#include <common/Common.h>
#include <common/Types.h>
#include <components/InternodeSyncer.h>
#include <components/AckManager.h>
#include <toolkit/ExternalHelperd.h>
#include <toolkit/InodeRefStore.h>
#include <toolkit/NoAllocBufferStore.h>


#define PROCFSHELPER_CONFIGKEYS_SIZE \
   ( sizeof(PROCFSHELPER_CONFIGKEYS) / sizeof(char*) )


const char* const PROCFSHELPER_CONFIGKEYS[] =
{
   "cfgFile",
   "logLevel",
   "logType",
   "logClientID",
   "logHelperdIP",
   "connUseRDMA",
   "connMgmtdPortUDP",
   "connClientPortUDP",
   "connMetaPortUDP",
   "connStoragePortUDP",
   "connMgmtdPortTCP",
   "connMetaPortTCP",
   "connStoragePortTCP",
   "connHelperdPortTCP",
   "connMaxInternodeNum",
   "connInterfacesFile",
   "connNetFilterFile",
   "connAuthFile",
   "connDisableAuthentication",
   "connTcpOnlyFilterFile",
   "connFallbackExpirationSecs",
   "connRDMABufSize",
   "connRDMABufNum",
   "connCommRetrySecs",
   "connNumCommRetries",
   "connUnmountRetries",
   "connMessagingTimeouts",
   "connRDMATimeouts",
   "tunePreferredMetaFile",
   "tunePreferredStorageFile",
   "tuneFileCacheType",
   "tuneFileCacheBufSize",
   "tuneRemoteFSync",
   "tuneUseGlobalFileLocks",
   "tuneRefreshOnGetAttr",
   "tuneInodeBlockBits",
   "tuneInodeBlockSize",
   "tuneEarlyCloseResponse",
   "tuneUseGlobalAppendLocks",
   "tuneUseBufferedAppend",
   "tuneStatFsCacheSecs",
   "tuneCoherentBuffers",
   "sysACLsEnabled",
   "sysMgmtdHost",
   "sysInodeIDStyle",
   "sysCreateHardlinksAsSymlinks",
   "sysMountSanityCheckMS",
   "sysSyncOnClose",
   "sysSessionCheckOnClose",
   "sysSessionChecksEnabled",
   "sysXAttrsEnabled",
   "quotaEnabled",
   "sysFileEventLogMask",
   "sysRenameEbusyAsXdev",

#ifdef LOG_DEBUG_MESSAGES
   "tunePageCacheValidityMS", // currently not public
   "tuneFileCacheBufNum", // currently not public
#endif // LOG_DEBUG_MESSAGES

   "" // just some final dummy element (for the syntactical comma-less end) with no effect
};


#define NIC_INDENTATION_STR        "+ "



int ProcFsHelper_readV2_config(struct seq_file* file, App* app)
{
   Config* cfg = App_getConfig(app);

   seq_printf(file, "cfgFile = %s\n", Config_getCfgFile(cfg) );
   seq_printf(file, "logLevel = %d\n", Config_getLogLevel(cfg) );
   seq_printf(file, "logType = %s\n", Config_logTypeNumToStr(Config_getLogTypeNum(cfg) ) );
   seq_printf(file, "logClientID = %d\n", (int)Config_getLogClientID(cfg) );
   seq_printf(file, "logHelperdIP = %s\n", Config_getLogHelperdIP(cfg) );
   seq_printf(file, "connUseRDMA = %d\n", (int)Config_getConnUseRDMA(cfg) );
   seq_printf(file, "connMgmtdPortUDP = %d\n", (int)Config_getConnMgmtdPortUDP(cfg) );
   seq_printf(file, "connClientPortUDP = %d\n", (int)Config_getConnClientPortUDP(cfg) );
   seq_printf(file, "connMgmtdPortTCP = %d\n", (int)Config_getConnMgmtdPortUDP(cfg) );
   seq_printf(file, "connHelperdPortTCP = %d\n", (int)Config_getConnHelperdPortTCP(cfg) );
   seq_printf(file, "connMaxInternodeNum = %u\n", Config_getConnMaxInternodeNum(cfg) );
   seq_printf(file, "connInterfacesFile = %s\n", Config_getConnInterfacesFile(cfg) );
   seq_printf(file, "connNetFilterFile = %s\n", Config_getConnNetFilterFile(cfg) );
   seq_printf(file, "connAuthFile = %s\n", Config_getConnAuthFile(cfg) );
   seq_printf(file, "connDisableAuthentication = %s\n",
      Config_getConnDisableAuthentication(cfg) ? "true" : "false");
   seq_printf(file, "connTcpOnlyFilterFile = %s\n", Config_getConnTcpOnlyFilterFile(cfg) );
   seq_printf(file, "connFallbackExpirationSecs = %u\n",
      Config_getConnFallbackExpirationSecs(cfg) );
   seq_printf(file, "connRDMABufSize = %u\n", Config_getConnRDMABufSize(cfg) );
   seq_printf(file, "connRDMABufNum = %u\n", Config_getConnRDMABufNum(cfg) );
   seq_printf(file, "connCommRetrySecs = %u\n", Config_getConnCommRetrySecs(cfg) );
   seq_printf(file, "connNumCommRetries = %u\n", Config_getConnNumCommRetries(cfg) );
   seq_printf(file, "connUnmountRetries = %d\n", (int)Config_getConnUnmountRetries(cfg) );
   seq_printf(file, "connMessagingTimeouts = %s\n", Config_getConnMessagingTimeouts(cfg) );
   seq_printf(file, "connRDMATimeouts = %s\n", Config_getConnRDMATimeouts(cfg) );
   seq_printf(file, "tunePreferredMetaFile = %s\n", Config_getTunePreferredMetaFile(cfg) );
   seq_printf(file, "tunePreferredStorageFile = %s\n", Config_getTunePreferredStorageFile(cfg) );
   seq_printf(file, "tuneFileCacheType = %s\n",
      Config_fileCacheTypeNumToStr(Config_getTuneFileCacheTypeNum(cfg) ) );
   seq_printf(file, "tuneFileCacheBufSize = %d\n", Config_getTuneFileCacheBufSize(cfg) );
   seq_printf(file, "tuneRemoteFSync = %d\n", (int)Config_getTuneRemoteFSync(cfg) );
   seq_printf(file, "tuneUseGlobalFileLocks = %d\n", (int)Config_getTuneUseGlobalFileLocks(cfg) );
   seq_printf(file, "tuneRefreshOnGetAttr = %d\n", (int)Config_getTuneRefreshOnGetAttr(cfg) );
   seq_printf(file, "tuneInodeBlockBits = %u\n", Config_getTuneInodeBlockBits(cfg) );
   seq_printf(file, "tuneInodeBlockSize = %u\n", Config_getTuneInodeBlockSize(cfg) );
   seq_printf(file, "tuneEarlyCloseResponse = %d\n", (int)Config_getTuneEarlyCloseResponse(cfg) );
   seq_printf(file, "tuneUseGlobalAppendLocks = %d\n",
      (int)Config_getTuneUseGlobalAppendLocks(cfg) );
   seq_printf(file, "tuneUseBufferedAppend = %d\n", (int)Config_getTuneUseBufferedAppend(cfg) );
   seq_printf(file, "tuneStatFsCacheSecs = %u\n", Config_getTuneStatFsCacheSecs(cfg) );
   seq_printf(file, "tuneCoherentBuffers = %u\n", Config_getTuneCoherentBuffers(cfg) );
   seq_printf(file, "sysACLsEnabled = %d\n", (int)Config_getSysACLsEnabled(cfg) );
   seq_printf(file, "sysMgmtdHost = %s\n", Config_getSysMgmtdHost(cfg) );
   seq_printf(file, "sysInodeIDStyle = %s\n",
      Config_inodeIDStyleNumToStr(Config_getSysInodeIDStyleNum(cfg) ) );
   seq_printf(file, "sysCreateHardlinksAsSymlinks = %d\n",
      (int)Config_getSysCreateHardlinksAsSymlinks(cfg) );
   seq_printf(file, "sysMountSanityCheckMS = %u\n", Config_getSysMountSanityCheckMS(cfg) );
   seq_printf(file, "sysSyncOnClose = %d\n", (int)Config_getSysSyncOnClose(cfg) );
   seq_printf(file, "sysXAttrsEnabled = %d\n", (int)Config_getSysXAttrsEnabled(cfg) );
   seq_printf(file, "sysSessionCheckOnClose = %d\n", (int)Config_getSysSessionCheckOnClose(cfg) );
   seq_printf(file, "sysSessionChecksEnabled = %d\n", (int)Config_getSysSessionChecksEnabled(cfg) );
   seq_printf(file, "quotaEnabled = %d\n", (int)Config_getQuotaEnabled(cfg) );
   seq_printf(file, "sysFileEventLogMask = %s\n", Config_eventLogMaskToStr(cfg->eventLogMask));
   seq_printf(file, "sysRenameEbusyAsXdev = %u\n", (unsigned) cfg->sysRenameEbusyAsXdev);


#ifdef LOG_DEBUG_MESSAGES
   seq_printf(file, "tunePageCacheValidityMS = %u\n", Config_getTunePageCacheValidityMS(cfg) );
   seq_printf(file, "tuneFileCacheBufNum = %d\n", Config_getTuneFileCacheBufNum(cfg) );
#endif // LOG_DEBUG_MESSAGES


   return 0;
}

int ProcFsHelper_read_config(char* buf, char** start, off_t offset, int size, int* eof, App* app)
{
   int count = 0;
   Config* cfg = App_getConfig(app);
   const char* currentKey = NULL;

   if(offset >= (off_t)PROCFSHELPER_CONFIGKEYS_SIZE)
   {
      *eof = 1;
      return 0;
   }

   currentKey = PROCFSHELPER_CONFIGKEYS[offset];


   if(!strcmp(currentKey, "cfgFile") )
      count = scnprintf(buf, size, "%s = %s\n", currentKey, Config_getCfgFile(cfg) );
   else
   if(!strcmp(currentKey, "logLevel") )
      count = scnprintf(buf, size, "%s = %d\n", currentKey, Config_getLogLevel(cfg) );
   else
   if(!strcmp(currentKey, "logType") )
      count = scnprintf(buf, size, "%s = %s\n", currentKey,
         Config_logTypeNumToStr(Config_getLogTypeNum(cfg) ) );
   else
   if(!strcmp(currentKey, "logClientID") )
      count = scnprintf(buf, size, "%s = %d\n", currentKey, (int)Config_getLogClientID(cfg) );
   else
   if(!strcmp(currentKey, "logHelperdIP") )
      count = scnprintf(buf, size, "%s = %s\n", currentKey, Config_getLogHelperdIP(cfg) );
   else
   if(!strcmp(currentKey, "connUseRDMA") )
      count = scnprintf(buf, size, "%s = %d\n", currentKey, (int)Config_getConnUseRDMA(cfg) );
   else
   if(!strcmp(currentKey, "connMgmtdPortUDP") )
      count = scnprintf(buf, size, "%s = %d\n",
         currentKey, (int)Config_getConnMgmtdPortUDP(cfg) );
   else
   if(!strcmp(currentKey, "connClientPortUDP") )
      count = scnprintf(buf, size, "%s = %d\n",
         currentKey, (int)Config_getConnClientPortUDP(cfg) );
   else
   if(!strcmp(currentKey, "connMgmtdPortTCP") )
      count = scnprintf(buf, size, "%s = %d\n",
         currentKey, (int)Config_getConnMgmtdPortUDP(cfg) );
   else
   if(!strcmp(currentKey, "connHelperdPortTCP") )
      count = scnprintf(buf, size, "%s = %d\n", currentKey,
         (int)Config_getConnHelperdPortTCP(cfg) );
   else
   if(!strcmp(currentKey, "connMaxInternodeNum") )
      count = scnprintf(buf, size, "%s = %u\n", currentKey, Config_getConnMaxInternodeNum(cfg) );
   else
   if(!strcmp(currentKey, "connInterfacesFile") )
      count = scnprintf(buf, size, "%s = %s\n",
         currentKey, Config_getConnInterfacesFile(cfg) );
   else
   if(!strcmp(currentKey, "connNetFilterFile") )
      count = scnprintf(buf, size, "%s = %s\n",
         currentKey, Config_getConnNetFilterFile(cfg) );
   else
   if(!strcmp(currentKey, "connTcpOnlyFilterFile") )
      count = scnprintf(buf, size, "%s = %s\n",
         currentKey, Config_getConnTcpOnlyFilterFile(cfg) );
   else
   if(!strcmp(currentKey, "connFallbackExpirationSecs") )
      count = scnprintf(buf, size, "%s = %u\n", currentKey,
         Config_getConnFallbackExpirationSecs(cfg) );
   else
   if(!strcmp(currentKey, "connRDMABufSize") )
      count = scnprintf(buf, size, "%s = %u\n", currentKey, Config_getConnRDMABufSize(cfg) );
   else
   if(!strcmp(currentKey, "connRDMABufNum") )
      count = scnprintf(buf, size, "%s = %u\n", currentKey, Config_getConnRDMABufNum(cfg) );
   else
   if(!strcmp(currentKey, "connCommRetrySecs") )
      count = scnprintf(buf, size, "%s = %u\n", currentKey, Config_getConnCommRetrySecs(cfg) );
   else
   if(!strcmp(currentKey, "connNumCommRetries") )
      count = scnprintf(buf, size, "%s = %u\n", currentKey, Config_getConnNumCommRetries(cfg) );
   else
   if(!strcmp(currentKey, "connUnmountRetries") )
      count = scnprintf(buf, size, "%s = %d\n", currentKey,
         (int)Config_getConnUnmountRetries(cfg) );
   else
   if(!strcmp(currentKey, "tunePreferredMetaFile") )
      count = scnprintf(buf, size, "%s = %s\n", currentKey,
         Config_getTunePreferredMetaFile(cfg) );
   else
   if(!strcmp(currentKey, "tunePreferredStorageFile") )
      count = scnprintf(buf, size, "%s = %s\n", currentKey,
         Config_getTunePreferredStorageFile(cfg) );
   else
   if(!strcmp(currentKey, "tuneFileCacheType") )
      count = scnprintf(buf, size, "%s = %s\n", currentKey,
         Config_fileCacheTypeNumToStr(Config_getTuneFileCacheTypeNum(cfg) ) );
   else
   if(!strcmp(currentKey, "tuneFileCacheBufSize") )
      count = scnprintf(buf, size, "%s = %d\n",
         currentKey, Config_getTuneFileCacheBufSize(cfg) );
   else
   if(!strcmp(currentKey, "tuneFileCacheBufNum") )
      count = scnprintf(buf, size, "%s = %d\n", currentKey, Config_getTuneFileCacheBufNum(cfg) );
   else
   if(!strcmp(currentKey, "tunePageCacheValidityMS") )
      count = scnprintf(buf, size, "%s = %u\n", currentKey,
         Config_getTunePageCacheValidityMS(cfg) );
   else
   if(!strcmp(currentKey, "tuneRemoteFSync") )
      count = scnprintf(buf, size, "%s = %d\n", currentKey, Config_getTuneRemoteFSync(cfg) );
   else
   if(!strcmp(currentKey, "tuneUseGlobalFileLocks") )
      count = scnprintf(buf, size, "%s = %d\n", currentKey,
         Config_getTuneUseGlobalFileLocks(cfg) );
   else
   if(!strcmp(currentKey, "tuneRefreshOnGetAttr") )
      count = scnprintf(buf, size, "%s = %d\n", currentKey,
         Config_getTuneRefreshOnGetAttr(cfg) );
   else
   if(!strcmp(currentKey, "tuneInodeBlockBits") )
      count = scnprintf(buf, size, "%s = %u\n", currentKey,
         Config_getTuneInodeBlockBits(cfg) );
   else
   if(!strcmp(currentKey, "tuneInodeBlockSize") )
      count = scnprintf(buf, size, "%s = %u\n", currentKey,
         Config_getTuneInodeBlockSize(cfg) );
   else
   if(!strcmp(currentKey, "tuneEarlyCloseResponse") )
      count = scnprintf(buf, size, "%s = %d\n", currentKey,
         Config_getTuneEarlyCloseResponse(cfg) );
   else
   if(!strcmp(currentKey, "tuneUseGlobalAppendLocks") )
      count = scnprintf(buf, size, "%s = %d\n", currentKey,
         Config_getTuneUseGlobalAppendLocks(cfg) );
   else
   if(!strcmp(currentKey, "tuneUseBufferedAppend") )
      count = scnprintf(buf, size, "%s = %d\n", currentKey,
         Config_getTuneUseBufferedAppend(cfg) );
   else
   if(!strcmp(currentKey, "tuneStatFsCacheSecs") )
      count = scnprintf(buf, size, "%s = %u\n", currentKey,
         Config_getTuneStatFsCacheSecs(cfg) );
   else
   if(!strcmp(currentKey, "tuneCoherentBuffers") )
      count = scnprintf(buf, size, "%s = %u\n", currentKey, Config_getTuneCoherentBuffers(cfg) );
   else
   if(!strcmp(currentKey, "sysACLsEnabled") )
      count = scnprintf(buf, size, "%s = %d\n", currentKey, Config_getSysACLsEnabled(cfg) );
   else
   if(!strcmp(currentKey, "sysMgmtdHost") )
      count = scnprintf(buf, size, "%s = %s\n", currentKey, Config_getSysMgmtdHost(cfg) );
   else
   if(!strcmp(currentKey, "sysInodeIDStyle") )
      count = scnprintf(buf, size, "%s = %s\n", currentKey,
         Config_inodeIDStyleNumToStr(Config_getSysInodeIDStyleNum(cfg) ) );
   else
   if(!strcmp(currentKey, "sysCreateHardlinksAsSymlinks") )
      count = scnprintf(buf, size, "%s = %d\n", currentKey,
         Config_getSysCreateHardlinksAsSymlinks(cfg) );
   else
   if(!strcmp(currentKey, "sysMountSanityCheckMS") )
      count = scnprintf(buf, size, "%s = %u\n", currentKey,
         Config_getSysMountSanityCheckMS(cfg) );
   else
   if(!strcmp(currentKey, "sysSyncOnClose") )
      count = scnprintf(buf, size, "%s = %u\n", currentKey,
         Config_getSysSyncOnClose(cfg) );
   else
   if(!strcmp(currentKey, "sysSessionCheckOnClose") )
      count = scnprintf(buf, size, "%s = %u\n", currentKey,
         Config_getSysSessionCheckOnClose(cfg) );
   else
   if(!strcmp(currentKey, "sysSessionChecksEnabled") )
      count = scnprintf(buf, size, "%s = %u\n", currentKey,
         Config_getSysSessionChecksEnabled(cfg) );
   else
   if(!strcmp(currentKey, "sysXAttrsEnabled") )
      count = scnprintf(buf, size, "%s = %d\n", currentKey, Config_getSysXAttrsEnabled(cfg) );
   else
   if (!strcmp(currentKey, "sysRenameEbusyAsXdev"))
      count = scnprintf(buf, size, "%s = %u\n", currentKey, (unsigned) cfg->sysRenameEbusyAsXdev);
   else
   if(!strcmp(currentKey, "quotaEnabled") )
      count = scnprintf(buf, size, "%s = %u\n", currentKey,
         Config_getQuotaEnabled(cfg) );
   else if (!strcmp(currentKey, "sysFileEventLogMask"))
      count = scnprintf(buf, size, "%s = %s\n", currentKey,
         Config_eventLogMaskToStr(cfg->eventLogMask));
   else
   { // undefined (or hidden) element
      // nothing to be done here
   }


   *start = (char*)1; // move to next offset (note: yes, we're casting a number to a pointer here!)

   return count;
}

int ProcFsHelper_readV2_status(struct seq_file* file, App* app)
{
   {
      NoAllocBufferStore* bufStore = App_getMsgBufStore(app);
      size_t numAvailableMsgBufs = NoAllocBufferStore_getNumAvailable(bufStore);

      seq_printf(file, "numAvailableMsgBufs: %u\n", (unsigned)numAvailableMsgBufs);
   }

   {
      InternodeSyncer* syncer = App_getInternodeSyncer(app);
      size_t delayedQSize = InternodeSyncer_getDelayedCloseQueueSize(syncer);

      seq_printf(file, "numDelayedClose: %u\n", (unsigned)delayedQSize);
   }

   {
      InternodeSyncer* syncer = App_getInternodeSyncer(app);
      size_t delayedQSize = InternodeSyncer_getDelayedEntryUnlockQueueSize(syncer);

      seq_printf(file, "numDelayedEntryUnlock: %u\n", (unsigned)delayedQSize);
   }

   {
      InternodeSyncer* syncer = App_getInternodeSyncer(app);
      size_t delayedQSize = InternodeSyncer_getDelayedRangeUnlockQueueSize(syncer);

      seq_printf(file, "numDelayedRangeUnlock: %u\n", (unsigned)delayedQSize);
   }

   {
      AckManager* ackMgr = App_getAckManager(app);
      size_t queueSize = AckManager_getAckQueueSize(ackMgr);

      seq_printf(file, "numEnqueuedAcks: %u\n", (unsigned)queueSize);
   }

   {
      NoAllocBufferStore* cacheStore = App_getCacheBufStore(app);
      if(cacheStore)
      {
         size_t numAvailable = NoAllocBufferStore_getNumAvailable(cacheStore);

         seq_printf(file, "numAvailableIOBufs: %u\n", (unsigned)numAvailable);
      }
   }

   {
      InodeRefStore* refStore = App_getInodeRefStore(app);
      if(refStore)
      {
         size_t storeSize = InodeRefStore_getSize(refStore);

         seq_printf(file, "numFlushRefs: %u\n", (unsigned)storeSize);
      }
   }

   #ifdef BEEGFS_DEBUG

   {
      unsigned long long numRPCs = App_getNumRPCs(app);
      seq_printf(file, "numRPCs: %llu\n", numRPCs);
   }

   {
      unsigned long long numRemoteReads = App_getNumRemoteReads(app);
      seq_printf(file, "numRemoteReads: %llu\n", numRemoteReads);
   }

   {
      unsigned long long numRemoteWrites = App_getNumRemoteWrites(app);
      seq_printf(file, "numRemoteWrites: %llu\n", numRemoteWrites);
   }

   #endif // BEEGFS_DEBUG

   return 0;
}

int ProcFsHelper_read_status(char* buf, char** start, off_t offset, int size, int* eof, App* app)
{
   int count = 0;
   int i=0; // just some virtual sequential index to match sequental offset

   if(offset == i++)
   {
      NoAllocBufferStore* bufStore = App_getMsgBufStore(app);
      size_t numAvailableMsgBufs = NoAllocBufferStore_getNumAvailable(bufStore);

      count = scnprintf(buf, size, "numAvailableMsgBufs: %u\n", (unsigned)numAvailableMsgBufs);
   }
   else
   if(offset == i++)
   {
      InternodeSyncer* syncer = App_getInternodeSyncer(app);
      size_t delayedQSize = InternodeSyncer_getDelayedCloseQueueSize(syncer);

      count = scnprintf(buf, size, "numDelayedClose: %u\n", (unsigned)delayedQSize);
   }
   else
   if(offset == i++)
   {
      InternodeSyncer* syncer = App_getInternodeSyncer(app);
      size_t delayedQSize = InternodeSyncer_getDelayedEntryUnlockQueueSize(syncer);

      count = scnprintf(buf, size, "numDelayedEntryUnlock: %u\n", (unsigned)delayedQSize);
   }
   else
   if(offset == i++)
   {
      InternodeSyncer* syncer = App_getInternodeSyncer(app);
      size_t delayedQSize = InternodeSyncer_getDelayedRangeUnlockQueueSize(syncer);

      count = scnprintf(buf, size, "numDelayedRangeUnlock: %u\n", (unsigned)delayedQSize);
   }
   else
   if(offset == i++)
   {
      AckManager* ackMgr = App_getAckManager(app);
      size_t queueSize = AckManager_getAckQueueSize(ackMgr);

      count = scnprintf(buf, size, "numEnqueuedAcks: %u\n", (unsigned)queueSize);
   }
   else
   if(offset == i++)
   {
      NoAllocBufferStore* cacheStore = App_getCacheBufStore(app);
      if(cacheStore)
      {
         size_t numAvailable = NoAllocBufferStore_getNumAvailable(cacheStore);

         count = scnprintf(buf, size, "numAvailableIOBufs: %u\n", (unsigned)numAvailable);
      }
   }
   else
   if(offset == i++)
   {
      InodeRefStore* refStore = App_getInodeRefStore(app);
      if(refStore)
      {
         size_t storeSize = InodeRefStore_getSize(refStore);

         count = scnprintf(buf, size, "numFlushRefs: %u\n", (unsigned)storeSize);
      }
   }
#ifdef BEEGFS_DEBUG
   else
   if(offset == i++)
   {
      unsigned long long numRPCs = App_getNumRPCs(app);
      count = scnprintf(buf, size, "numRPCs: %llu\n", numRPCs);
   }
   else
   if(offset == i++)
   {
      unsigned long long numRemoteReads = App_getNumRemoteReads(app);
      count = scnprintf(buf, size, "numRemoteReads: %llu\n", numRemoteReads);
   }
   else
   if(offset == i++)
   {
      unsigned long long numRemoteWrites = App_getNumRemoteWrites(app);
      count = scnprintf(buf, size, "numRemoteWrites: %llu\n", numRemoteWrites);
   }
#endif // BEEGFS_DEBUG
   else
   {
      *eof = 1;
      return 0;
   }


   *start = (char*)1; // move to next offset (note: yes, we're casting a number to a pointer here!)

   return count;
}

/**
 * @param nodes show nodes from this store
 */
int ProcFsHelper_readV2_nodes(struct seq_file* file, App* app, struct NodeStoreEx* nodes)
{
   Node* currentNode = NodeStoreEx_referenceFirstNode(nodes);

   while(currentNode)
   {
      NumNodeID numID = Node_getNumID(currentNode);
      const char* nodeID = NumNodeID_str(&numID);

      seq_printf(file, "%s [ID: %s]\n", Node_getID(currentNode),  nodeID);
      kfree(nodeID);

      __ProcFsHelper_printGotRootV2(file, currentNode, nodes);
      __ProcFsHelper_printNodeConnsV2(file, currentNode);

      // next node
      currentNode = NodeStoreEx_referenceNextNodeAndReleaseOld(nodes, currentNode);
   }

   return 0;
}


/**
 * Note: This does not guarantee to return the complete list (e.g. in case the list changes during
 * two calls to this method)
 *
 * @param nodes show nodes from this store
 */
int ProcFsHelper_read_nodes(char* buf, char** start, off_t offset, int size, int* eof,
   struct NodeStoreEx* nodes)
{
   Node* currentNode = NULL;
   off_t currentOffset = 0;
   int count = 0;
   NumNodeID nodeNumID;
   const char* nodeID;

   currentNode = NodeStoreEx_referenceFirstNode(nodes);
   while(currentNode && (currentOffset < offset) )
   {
      currentNode = NodeStoreEx_referenceNextNodeAndReleaseOld(nodes, currentNode);
      currentOffset++;
   }

   if(!currentNode)
   { // this offset does not exist
      *eof = 1;
      return 0;
   }

   nodeNumID = Node_getNumID(currentNode);
   nodeID = NumNodeID_str(&nodeNumID);
   count += scnprintf(buf+count, size-count, "%s [ID: %s]\n", Node_getID(currentNode), nodeID);
   kfree(nodeID);

   __ProcFsHelper_printGotRoot(currentNode, nodes, buf, &count, &size);
   __ProcFsHelper_printNodeConns(currentNode, buf, &count, &size);

   *start = (char*)1; // move to next offset (note: yes, we're casting a number to a pointer here!)

   Node_put(currentNode);

   return count;
}

int ProcFsHelper_readV2_clientInfo(struct seq_file* file, App* app)
{
   Node* localNode = App_getLocalNode(app);
   char* localNodeID = Node_getID(localNode);

   NicAddressList* nicList = Node_getNicList(localNode);
   NicAddressListIter nicIter;

   // print local clientID

   seq_printf(file, "ClientID: %s\n", localNodeID);

   // list usable network interfaces

   seq_printf(file, "Interfaces:\n");

   NicAddressListIter_init(&nicIter, nicList);

   for( ; !NicAddressListIter_end(&nicIter); NicAddressListIter_next(&nicIter) )
   {
      NicAddress* nicAddr = NicAddressListIter_value(&nicIter);
      char* nicAddrStr = NIC_nicAddrToString(nicAddr);

      if(unlikely(!nicAddrStr) )
         seq_printf(file, "%sNIC not available [Error: Out of memory]\n", NIC_INDENTATION_STR);
      else
      {
         seq_printf(file, "%s%s\n", NIC_INDENTATION_STR, nicAddrStr);

         kfree(nicAddrStr);
      }
   }

   return 0;
}


int ProcFsHelper_read_clientInfo(char* buf, char** start, off_t offset, int size, int* eof,
   App* app)
{
   int count = 0;

   Node* localNode = App_getLocalNode(app);
   char* localNodeID = Node_getID(localNode);

   size_t nicListStrLen = 1024;
   char* extendedNicListStr = vmalloc(nicListStrLen);
   NicAddressList* nicList = Node_getNicList(localNode);
   NicAddressListIter nicIter;


   // print local clientID
   count += scnprintf(buf+count, size-count, "ClientID: %s\n", localNodeID);

   // list usable network interfaces
   NicAddressListIter_init(&nicIter, nicList);
   extendedNicListStr[0] = 0;

   for( ; !NicAddressListIter_end(&nicIter); NicAddressListIter_next(&nicIter) )
   {
      char* nicAddrStr;
      NicAddress* nicAddr = NicAddressListIter_value(&nicIter);
      char* tmpStr = vmalloc(nicListStrLen);

      // extended NIC info
      nicAddrStr = NIC_nicAddrToString(nicAddr);
      snprintf(tmpStr, nicListStrLen, "%s%s%s\n", extendedNicListStr, NIC_INDENTATION_STR,
         nicAddrStr);
      strcpy(extendedNicListStr, tmpStr); // tmpStr to avoid writing to a buffer that we're
         // reading from at the same time
      kfree(nicAddrStr);

      SAFE_VFREE(tmpStr);
   }

   count += scnprintf(buf+count, size-count, "Interfaces:\n%s", extendedNicListStr);

   // clean up
   SAFE_VFREE(extendedNicListStr);

   *eof = 1;

   return count;
}

int ProcFsHelper_readV2_targetStates(struct seq_file* file, App* app,
   struct TargetStateStore* targetsStates, struct NodeStoreEx* nodes, bool isMeta)
{
   FhgfsOpsErr inOutError = FhgfsOpsErr_SUCCESS;

   UInt8List reachabilityStates;
   UInt8List consistencyStates;
   UInt16List targetIDs;

   UInt8ListIter reachabilityStatesIter;
   UInt8ListIter consistencyStatesIter;
   UInt16ListIter targetIDsIter;

   Node* currentNode = NULL;

   TargetMapper* targetMapper = App_getTargetMapper(app);

   UInt8List_init(&reachabilityStates);
   UInt8List_init(&consistencyStates);
   UInt16List_init(&targetIDs);

   TargetStateStore_getStatesAsLists(targetsStates, &targetIDs,
      &reachabilityStates, &consistencyStates);

   for(UInt16ListIter_init(&targetIDsIter, &targetIDs),
         UInt8ListIter_init(&reachabilityStatesIter, &reachabilityStates),
         UInt8ListIter_init(&consistencyStatesIter, &consistencyStates);
      (!UInt16ListIter_end(&targetIDsIter) )
         && (!UInt8ListIter_end(&reachabilityStatesIter) )
         && (!UInt8ListIter_end(&consistencyStatesIter) );
      UInt16ListIter_next(&targetIDsIter),
         UInt8ListIter_next(&reachabilityStatesIter),
         UInt8ListIter_next(&consistencyStatesIter) )
   {
      if(isMeta)
      { // meta nodes
         currentNode = NodeStoreEx_referenceNode(nodes,
            (NumNodeID){ UInt16ListIter_value(&targetIDsIter) });
      }
      else
      { // storage nodes
         currentNode = NodeStoreEx_referenceNodeByTargetID(nodes,
            UInt16ListIter_value(&targetIDsIter), targetMapper, &inOutError);
      }

      if(unlikely(!currentNode) )
      { // this offset does not exist
         UInt8List_uninit(&consistencyStates);
         UInt8List_uninit(&reachabilityStates);
         UInt16List_uninit(&targetIDs);

         return 0;
      }

      seq_printf(file, "[Target ID: %hu] @ %s: %s / %s \n", UInt16ListIter_value(&targetIDsIter),
         Node_getNodeIDWithTypeStr(currentNode),
         TargetStateStore_reachabilityStateToStr(UInt8ListIter_value(&reachabilityStatesIter) ),
         TargetStateStore_consistencyStateToStr(UInt8ListIter_value(&consistencyStatesIter) ) );

      Node_put(currentNode);
   }

   UInt8List_uninit(&reachabilityStates);
   UInt8List_uninit(&consistencyStates);
   UInt16List_uninit(&targetIDs);

   return 0;
}

int ProcFsHelper_read_targetStates(char* buf, char** start, off_t offset, int size, int* eof,
   App* app, struct TargetStateStore* targetsStates, struct NodeStoreEx* nodes, bool isMeta)
{
   off_t currentOffset = 0;
   int count = 0;

   FhgfsOpsErr inOutError = FhgfsOpsErr_SUCCESS;

   Node* currentNode = NULL;

   UInt8List reachabilityStates;
   UInt8List consistencyStates;
   UInt16List targetIDs;

   UInt8ListIter reachabilityStatesIter;
   UInt8ListIter consistencyStatesIter;
   UInt16ListIter targetIDsIter;

   TargetMapper* targetMapper = App_getTargetMapper(app);

   UInt8List_init(&reachabilityStates);
   UInt8List_init(&consistencyStates);
   UInt16List_init(&targetIDs);

   TargetStateStore_getStatesAsLists(targetsStates, &targetIDs,
      &reachabilityStates, &consistencyStates);

   UInt16ListIter_init(&targetIDsIter, &targetIDs);
   UInt8ListIter_init(&reachabilityStatesIter, &reachabilityStates);
   UInt8ListIter_init(&consistencyStatesIter, &consistencyStates);

   while( (!UInt16ListIter_end(&targetIDsIter) )
      && (!UInt8ListIter_end(&reachabilityStatesIter) )
      && (!UInt8ListIter_end(&consistencyStatesIter) )
      && (currentOffset < offset) )
   {
      currentOffset++;

      UInt16ListIter_next(&targetIDsIter);
      UInt8ListIter_next(&reachabilityStatesIter);
      UInt8ListIter_next(&consistencyStatesIter);
   }

   if(UInt16ListIter_end(&targetIDsIter) )
   { // this offset does not exist
      UInt8List_uninit(&consistencyStates);
      UInt8List_uninit(&reachabilityStates);
      UInt16List_uninit(&targetIDs);

      *eof = 1;
      return 0;
   }

   if(isMeta)
   { // meta nodes
      currentNode = NodeStoreEx_referenceNode(nodes,
         (NumNodeID){UInt16ListIter_value(&targetIDsIter)} );
   }
   else
   { // storage nodes
      currentNode = NodeStoreEx_referenceNodeByTargetID(nodes, UInt16ListIter_value(&targetIDsIter),
         targetMapper, &inOutError);
   }

   if(unlikely(!currentNode) )
   {
      UInt8List_uninit(&consistencyStates);
      UInt8List_uninit(&reachabilityStates);
      UInt16List_uninit(&targetIDs);

      *eof = 1;
      return 0;
   }

   count += scnprintf(buf+count, size-count, "[Target ID: %hu] @ %s: %s / %s \n",
      UInt16ListIter_value(&targetIDsIter), Node_getNodeIDWithTypeStr(currentNode),
      TargetStateStore_reachabilityStateToStr(UInt8ListIter_value(&reachabilityStatesIter) ),
      TargetStateStore_consistencyStateToStr(UInt8ListIter_value(&consistencyStatesIter) ) );

   Node_put(currentNode);

   *start = (char*)1; // move to next offset (note: yes, we're casting a number to a pointer here!)

   UInt8List_uninit(&consistencyStates);
   UInt8List_uninit(&reachabilityStates);
   UInt16List_uninit(&targetIDs);

   return count;
}

int ProcFsHelper_readV2_connRetriesEnabled(struct seq_file* file, App* app)
{
   bool retriesEnabled = App_getConnRetriesEnabled(app);

   seq_printf(file, "%d\n", retriesEnabled ? 1 : 0);

   return 0;
}

int ProcFsHelper_read_connRetriesEnabled(char* buf, char** start, off_t offset, int size, int* eof,
   App* app)
{
   int count = 0;

   bool retriesEnabled = App_getConnRetriesEnabled(app);

   count += scnprintf(buf+count, size-count, "%d\n", retriesEnabled ? 1 : 0);

   *eof = 1;

   return count;
}

int ProcFsHelper_write_connRetriesEnabled(const char __user *buf, unsigned long count, App* app)
{
   int retVal;
   long copyVal;

   char* kernelBuf;
   char* trimCopy;
   bool retriesEnabled;

   kernelBuf = os_kmalloc(count+1); // +1 for zero-term
   kernelBuf[count] = 0;

   copyVal = __copy_from_user(kernelBuf, buf, count);
   if (copyVal != 0)
   {
      retVal = -EFAULT;
      goto out_fault;
   }

   trimCopy = StringTk_trimCopy(kernelBuf);

   retriesEnabled = StringTk_strToBool(trimCopy);
   App_setConnRetriesEnabled(app, retriesEnabled);

   retVal = count;

   kfree(trimCopy);

out_fault:
   kfree(kernelBuf);

   return retVal;
}

int ProcFsHelper_read_remapConnectionFailure(struct seq_file* file, App* app)
{
   Config* cfg = App_getConfig(app);
   unsigned remapConnectionFailure = Config_getRemapConnectionFailureStatus(cfg);

   seq_printf(file, "%d\n", remapConnectionFailure);

   return 0;
}

int ProcFsHelper_write_remapConnectionFailure(const char __user *buf, unsigned long count, App* app)
{
   int retVal;
   long copyVal;
   Config* cfg = App_getConfig(app);

   char* kernelBuf;
   char* trimCopy;
   unsigned remapConnectionFailure;

   kernelBuf = os_kmalloc(count+1); // +1 for zero-term
   kernelBuf[count] = 0;

   copyVal = __copy_from_user(kernelBuf, buf, count);
   if (copyVal != 0)
   {
      retVal = -EFAULT;
      goto out_fault;
   }

   trimCopy = StringTk_trimCopy(kernelBuf);

   remapConnectionFailure = StringTk_strToInt(trimCopy);
   Config_setRemapConnectionFailureStatus(cfg, remapConnectionFailure);

   retVal = count;

   kfree(trimCopy);

out_fault:
   kfree(kernelBuf);

   return retVal;
}

int ProcFsHelper_readV2_netBenchModeEnabled(struct seq_file* file, App* app)
{
   bool netBenchModeEnabled = App_getNetBenchModeEnabled(app);

   seq_printf(file,
      "%d\n"
      "# Enabled netbench_mode (=1) means that storage servers will not read from or\n"
      "# write to the underlying file system, so that network throughput can be tested\n"
      "# independent of storage device throughput from a netbench client.\n",
      netBenchModeEnabled ? 1 : 0);

   return 0;
}

int ProcFsHelper_read_netBenchModeEnabled(char* buf, char** start, off_t offset, int size, int* eof,
   App* app)
{
   int count = 0;

   bool netBenchModeEnabled = App_getNetBenchModeEnabled(app);

   count += scnprintf(buf+count, size-count,
      "%d\n"
      "# Enabled netbench_mode (=1) means that storage servers will not read from or\n"
      "# write to the underlying file system, so that network throughput can be tested\n"
      "# independent of storage device throughput from a netbench client.\n",
      netBenchModeEnabled ? 1 : 0);

   *eof = 1;

   return count;
}

int ProcFsHelper_write_netBenchModeEnabled(const char __user *buf, unsigned long count, App* app)
{
   const char* logContext = "procfs (netbench mode)";

   int retVal;
   long copyVal;
   Logger* log = App_getLogger(app);

   char* kernelBuf;
   char* trimCopy;
   bool netBenchModeOldValue;
   bool netBenchModeEnabled; // new value

   kernelBuf = os_kmalloc(count+1); // +1 for zero-term
   kernelBuf[count] = 0;

   copyVal = __copy_from_user(kernelBuf, buf, count);
   if (copyVal != 0)
   {
      retVal = -EFAULT;
      goto out_fault;
   }

   trimCopy = StringTk_trimCopy(kernelBuf);

   netBenchModeEnabled = StringTk_strToBool(trimCopy);

   netBenchModeOldValue = App_getNetBenchModeEnabled(app);
   App_setNetBenchModeEnabled(app, netBenchModeEnabled);

   if(netBenchModeOldValue != netBenchModeEnabled)
   { // print log message
      if(netBenchModeEnabled)
         Logger_log(log, Log_CRITICAL, logContext, "Network benchmark mode enabled. "
            "Storage servers will not write to or read from underlying file system.");
      else
         Logger_log(log, Log_CRITICAL, logContext, "Network benchmark mode disabled.");
   }

   retVal = count;

   kfree(trimCopy);

out_fault:
   kfree(kernelBuf);

   return retVal;
}

/**
 * Drop all established (available) connections.
 */
int ProcFsHelper_write_dropConns(const char __user *buf, unsigned long count, App* app)
{
   ExternalHelperd* helperd = App_getHelperd(app);
   NodeStoreEx* mgmtNodes = App_getMgmtNodes(app);
   NodeStoreEx* metaNodes = App_getMetaNodes(app);
   NodeStoreEx* storageNodes = App_getStorageNodes(app);

   ExternalHelperd_dropConns(helperd);

   NodesTk_dropAllConnsByStore(mgmtNodes);
   NodesTk_dropAllConnsByStore(metaNodes);
   NodesTk_dropAllConnsByStore(storageNodes);

   return count;
}

/**
 * Print all log topics and their current log level.
 */
int ProcFsHelper_readV2_logLevels(struct seq_file* file, App* app)
{
   Logger* log = App_getLogger(app);

   unsigned currentTopicNum;

   for(currentTopicNum = 0; currentTopicNum < LogTopic_LAST; currentTopicNum++)
   {
      const char* logTopicStr = Logger_getLogTopicStr( (LogTopic)currentTopicNum);
      int logLevel = Logger_getLogTopicLevel(log, (LogTopic)currentTopicNum);

      seq_printf(file, "%s = %d\n", logTopicStr, logLevel);
   }

   return 0;
}

/**
 * Print all log topics and their current log level.
 */
int ProcFsHelper_read_logLevels(char* buf, char** start, off_t offset, int size, int* eof, App* app)
{
   Logger* log = App_getLogger(app);

   int count = 0;

   if(offset < LogTopic_LAST)
   {
      const char* logTopicStr = Logger_getLogTopicStr( (LogTopic)offset);
      int logLevel = Logger_getLogTopicLevel(log, (LogTopic)offset);

      count = scnprintf(buf, size, "%s = %d\n", logTopicStr, logLevel);
   }
   else
   {
      *eof = 1;
      return 0;
   }


   *start = (char*)1; // move to next offset (note: yes, we're casting a number to a pointer here!)

   return count;
}

/**
 * Set a new log level for a certain log topic.
 *
 * Note: Expects a string of format "<topic>=<level>".
 */
int ProcFsHelper_write_logLevels(const char __user *buf, unsigned long count, App* app)
{
   Logger* log = App_getLogger(app);

   int retVal = -EINVAL;
   long copyVal;

   StrCpyList stringList;
   StrCpyListIter iter;
   char* kernelBuf = NULL;
   char* trimCopy = NULL;

   const char* logTopicStr;
   LogTopic logTopic;
   const char* logLevelStr;
   int logLevel;

   StrCpyList_init(&stringList);

   // copy user string to kernel buffer
   kernelBuf = os_kmalloc(count+1); // +1 for zero-term
   kernelBuf[count] = 0;

   copyVal = __copy_from_user(kernelBuf, buf, count);
   if (copyVal != 0)
   {
      retVal = -EFAULT;
      goto out_fault;
   }

   trimCopy = StringTk_trimCopy(kernelBuf);

   StringTk_explode(trimCopy, '=', &stringList);

   if(StrCpyList_length(&stringList) != 2)
      goto cleanup_list_and_exit; // bad input string

   // now the string list contains topic and new level

   StrCpyListIter_init(&iter, &stringList);

   logTopicStr = StrCpyListIter_value(&iter);
   if(!Logger_getLogTopicFromStr(logTopicStr, &logTopic) )
      goto cleanup_list_and_exit;

   StrCpyListIter_next(&iter);

   logLevelStr = StrCpyListIter_value(&iter);

   if(sscanf(logLevelStr, "%d", &logLevel) != 1)
      goto cleanup_list_and_exit;

   Logger_setLogTopicLevel(log, logTopic, logLevel);

   retVal = count;

cleanup_list_and_exit:
   kfree(trimCopy);
   StrCpyList_uninit(&stringList);
out_fault:
   kfree(kernelBuf);

   return retVal;
}


void __ProcFsHelper_printGotRootV2(struct seq_file* file, Node* node, NodeStoreEx* nodes)
{
   NodeOrGroup rootOwner = NodeStoreEx_getRootOwner(nodes);

   if (rootOwner.group == Node_getNumID(node).value)
      seq_printf(file, "   Root: <yes>\n");
}

void __ProcFsHelper_printGotRoot(Node* node, NodeStoreEx* nodes, char* buf, int* pcount,
   int* psize)
{
   int count = *pcount;
   int size = *psize;

   NodeOrGroup rootOwner = NodeStoreEx_getRootOwner(nodes);

   if (rootOwner.group == Node_getNumID(node).value)
      count += scnprintf(buf+count, size-count, "   Root: <yes>\n");

   // set out values
   *pcount = count;
   *psize = size;
}

/**
 * Prints number of connections for each conn type and adds the peer name of an available conn
 * to each type.
 */
void __ProcFsHelper_printNodeConnsV2(struct seq_file* file, struct Node* node)
{
   NodeConnPool* connPool = Node_getConnPool(node);
   NodeConnPoolStats poolStats;

   const char* nonPrimaryTag = " [fallback route]"; // extra text to hint at non-primary conns
   bool isNonPrimaryConn;

   const unsigned peerNameBufLen = 64; /* 64 is just some arbitrary size that should be big enough
                                          to store the peer name */
   char* peerNameBuf = os_kmalloc(peerNameBufLen);

   if(unlikely(!peerNameBuf) )
   { // out of mem
      seq_printf(file, "   Connections not available [Error: Out of memory]\n");
      return;
   }

   seq_printf(file, "   Connections: ");


   NodeConnPool_getStats(connPool, &poolStats);

   if(!(poolStats.numEstablishedStd +
        poolStats.numEstablishedSDP +
        poolStats.numEstablishedRDMA) )
      seq_printf(file, "<none>");
   else
   { // print "<type>: <count> (<first_avail_peername>);"
      if(poolStats.numEstablishedStd)
      {
         if(unlikely(!peerNameBuf) )
            seq_printf(file, "TCP: %u; ", poolStats.numEstablishedStd);
         else
         {
            NodeConnPool_getFirstPeerName(connPool, NICADDRTYPE_STANDARD,
               peerNameBufLen, peerNameBuf, &isNonPrimaryConn);
            seq_printf(file, "TCP: %u (%s%s); ", poolStats.numEstablishedStd, peerNameBuf,
               isNonPrimaryConn ? nonPrimaryTag : "");
         }
      }

      if(poolStats.numEstablishedSDP)
      {
         if(unlikely(!peerNameBuf) )
            seq_printf(file, "SDP: %u; ", poolStats.numEstablishedSDP);
         else
         {
            NodeConnPool_getFirstPeerName(connPool, NICADDRTYPE_SDP,
               peerNameBufLen, peerNameBuf, &isNonPrimaryConn);
            seq_printf(file, "SDP: %u (%s%s); ", poolStats.numEstablishedSDP, peerNameBuf,
               isNonPrimaryConn ? nonPrimaryTag : "");
         }
      }

      if(poolStats.numEstablishedRDMA)
      {
         if(unlikely(!peerNameBuf) )
            seq_printf(file, "RDMA: %u; ", poolStats.numEstablishedRDMA);
         else
         {
            NodeConnPool_getFirstPeerName(connPool, NICADDRTYPE_RDMA,
               peerNameBufLen, peerNameBuf, &isNonPrimaryConn);
            seq_printf(file, "RDMA: %u (%s%s); ", poolStats.numEstablishedRDMA, peerNameBuf,
               isNonPrimaryConn ? nonPrimaryTag : "");
         }
      }
   }

   seq_printf(file, "\n");

   // cleanup
   SAFE_KFREE(peerNameBuf);
}

/**
 * Prints number of connections for each conn type and adds the peer name of an available conn
 * to each type.
 */
void __ProcFsHelper_printNodeConns(Node* node, char* buf, int* pcount, int* psize)
{
   int count = *pcount;
   int size = *psize;

   NodeConnPool* connPool;
   NodeConnPoolStats poolStats;

   const char* nonPrimaryTag = " [fallback route]"; // extra text to hint at non-primary conns
   bool isNonPrimaryConn;

   connPool = Node_getConnPool(node);
   NodeConnPool_getStats(connPool, &poolStats);

   count += scnprintf(buf+count, size-count, "   Connections: ");


   if(!(poolStats.numEstablishedStd +
        poolStats.numEstablishedSDP +
        poolStats.numEstablishedRDMA) )
      count += scnprintf(buf+count, size-count, "<none>");
   else
   { // print "<type>: <count> (<first_avail_peername>);"
      if(poolStats.numEstablishedStd)
      {
         count += scnprintf(buf+count, size-count, "TCP: %u (", poolStats.numEstablishedStd);
         count += NodeConnPool_getFirstPeerName(connPool, NICADDRTYPE_STANDARD,
            size-count, buf+count, &isNonPrimaryConn);
         count += scnprintf(buf+count, size-count, "%s", isNonPrimaryConn ? nonPrimaryTag : "");
         count += scnprintf(buf+count, size-count, "); ");
      }

      if(poolStats.numEstablishedSDP)
      {
         count += scnprintf(buf+count, size-count, "SDP: %u (", poolStats.numEstablishedSDP);
         count += NodeConnPool_getFirstPeerName(connPool, NICADDRTYPE_SDP,
            size-count, buf+count, &isNonPrimaryConn);
         count += scnprintf(buf+count, size-count, "%s", isNonPrimaryConn ? nonPrimaryTag : "");
         count += scnprintf(buf+count, size-count, "); ");
      }

      if(poolStats.numEstablishedRDMA)
      {
         count += scnprintf(buf+count, size-count, "RDMA: %u (", poolStats.numEstablishedRDMA);
         count += NodeConnPool_getFirstPeerName(connPool, NICADDRTYPE_RDMA,
            size-count, buf+count, &isNonPrimaryConn);
         count += scnprintf(buf+count, size-count, "%s", isNonPrimaryConn ? nonPrimaryTag : "");
         count += scnprintf(buf+count, size-count, "); ");
      }
   }

   count += scnprintf(buf+count, size-count, "\n");

   // set out values
   *pcount = count;
   *psize = size;
}

const char* ProcFsHelper_getSessionID(App* app)
{

   Node* localNode = App_getLocalNode(app);
   char* localNodeID = Node_getID(localNode);

   return localNodeID;
}
