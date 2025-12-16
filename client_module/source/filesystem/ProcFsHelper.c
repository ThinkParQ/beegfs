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
   seq_printf(file, "connTCPFallbackEnabled = %d\n", (int)Config_getConnTCPFallbackEnabled(cfg) );
   seq_printf(file, "connMgmtdPortUDP = %d\n", (int)Config_getConnMgmtdPortUDP(cfg) );
   seq_printf(file, "connClientPortUDP = %d\n", (int)Config_getConnClientPortUDP(cfg) );
   seq_printf(file, "connMgmtdPortTCP = %d\n", (int)Config_getConnMgmtdPortUDP(cfg) );
   seq_printf(file, "connHelperdPortTCP = %d\n", (int)Config_getConnHelperdPortTCP(cfg) );
   seq_printf(file, "connMaxInternodeNum = %u\n", Config_getConnMaxInternodeNum(cfg) );
   seq_printf(file, "connMaxConcurrentAttempts = %u\n", Config_getConnMaxConcurrentAttempts(cfg) );
   seq_printf(file, "connInterfacesFile = %s\n", Config_getConnInterfacesFile(cfg) );
   seq_printf(file, "connRDMAInterfacesFile = %s\n", Config_getConnRDMAInterfacesFile(cfg) );
   seq_printf(file, "connNetFilterFile = %s\n", Config_getConnNetFilterFile(cfg) );
   seq_printf(file, "connAuthFile = %s\n", Config_getConnAuthFile(cfg) );
   seq_printf(file, "connDisableAuthentication = %s\n",
      Config_getConnDisableAuthentication(cfg) ? "true" : "false");
   seq_printf(file, "connTcpOnlyFilterFile = %s\n", Config_getConnTcpOnlyFilterFile(cfg) );
   seq_printf(file, "connFallbackExpirationSecs = %u\n",
      Config_getConnFallbackExpirationSecs(cfg) );
   seq_printf(file, "connTCPRecvBufSize = %d\n", Config_getConnTCPRcvBufSize(cfg) );
   seq_printf(file, "connUDPRecvBufSize = %d\n", Config_getConnUDPRcvBufSize(cfg) );
   seq_printf(file, "connRDMABufSize = %u\n", Config_getConnRDMABufSize(cfg) );
   seq_printf(file, "connRDMAFragmentSize = %u\n", Config_getConnRDMAFragmentSize(cfg) );
   seq_printf(file, "connRDMABufNum = %u\n", Config_getConnRDMABufNum(cfg) );
   seq_printf(file, "connRDMAMetaBufSize = %u\n", Config_getConnRDMAMetaBufSize(cfg) );
   seq_printf(file, "connRDMAMetaFragmentSize = %u\n", Config_getConnRDMAMetaFragmentSize(cfg) );
   seq_printf(file, "connRDMAMetaBufNum = %u\n", Config_getConnRDMAMetaBufNum(cfg) );
   seq_printf(file, "connRDMATypeOfService = %d\n", Config_getConnRDMATypeOfService(cfg) );
   seq_printf(file, "connRDMAKeyType = %s\n",
      Config_rdmaKeyTypeNumToStr(Config_getConnRDMAKeyTypeNum(cfg)) );
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
   seq_printf(file, "tuneFileCacheBufNum = %d\n", Config_getTuneFileCacheBufNum(cfg) );
   seq_printf(file, "tunePageCacheValidityMS = %u\n", Config_getTunePageCacheValidityMS(cfg) );
   seq_printf(file, "tuneDirSubentryCacheValidityMS = %u\n", Config_getTuneDirSubentryCacheValidityMS(cfg) );
   seq_printf(file, "tuneFileSubentryCacheValidityMS = %u\n", Config_getTuneFileSubentryCacheValidityMS(cfg) );
   seq_printf(file, "tuneENOENTCacheValidityMS = %u\n", Config_getTuneENOENTCacheValidityMS(cfg) );
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
   seq_printf(file, "sysACLsRevalidate = %s\n",
      Config_ACLsRevalidateToStr(Config_getSysACLsRevalidate(cfg) ) );
   seq_printf(file, "sysMgmtdHost = %s\n", Config_getSysMgmtdHost(cfg) );
   seq_printf(file, "sysInodeIDStyle = %s\n",
      Config_inodeIDStyleNumToStr(Config_getSysInodeIDStyleNum(cfg) ) );
   seq_printf(file, "sysCreateHardlinksAsSymlinks = %d\n",
      (int)Config_getSysCreateHardlinksAsSymlinks(cfg) );
   seq_printf(file, "sysMountSanityCheckMS = %u\n", Config_getSysMountSanityCheckMS(cfg) );
   seq_printf(file, "sysSyncOnClose = %d\n", (int)Config_getSysSyncOnClose(cfg) );
   seq_printf(file, "sysXAttrsEnabled = %d\n", (int)Config_getSysXAttrsEnabled(cfg) );
   seq_printf(file, "sysXAttrsCheckCapabilities = %s\n",
      Config_checkCapabilitiesTypeToStr(Config_getSysXAttrsCheckCapabilities(cfg) ) );
   seq_printf(file, "sysSessionCheckOnClose = %d\n", (int)Config_getSysSessionCheckOnClose(cfg) );
   seq_printf(file, "sysSessionChecksEnabled = %d\n", (int)Config_getSysSessionChecksEnabled(cfg) );
   seq_printf(file, "sysUpdateTargetStatesSecs = %u\n", Config_getSysUpdateTargetStatesSecs(cfg) );
   seq_printf(file, "sysTargetOfflineTimeoutSecs = %u\n", Config_getSysTargetOfflineTimeoutSecs(cfg) );
   seq_printf(file, "quotaEnabled = %d\n", (int)Config_getQuotaEnabled(cfg) );
   seq_printf(file, "sysFileEventLogMask = %s\n", Config_eventLogMaskToStr(cfg->eventLogMask));
   seq_printf(file, "sysRenameEbusyAsXdev = %u\n", (unsigned) cfg->sysRenameEbusyAsXdev);
   seq_printf(file, "sysNoEnterpriseFeatureMsg = %s\n",
      Config_getSysNoEnterpriseFeatureMsg(cfg) ? "true" : "false");

   return 0;
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

static void ProcFsHelper_readV2_nics(struct seq_file* file, NicAddressList* nicList)
{
   NicAddressListIter nicIter;

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
}

int ProcFsHelper_readV2_clientInfo(struct seq_file* file, App* app)
{
   Node* localNode = App_getLocalNode(app);
   char* localNodeID = Node_getID(localNode);

   NicAddressList nicList;
   NicAddressList rdmaNicList;

   Node_cloneNicList(localNode, &nicList);
   App_cloneLocalRDMANicList(app, &rdmaNicList);
   // print local clientID

   seq_printf(file, "ClientID: %s\n", localNodeID);

   // list usable network interfaces

   seq_printf(file, "Interfaces:\n");
   ProcFsHelper_readV2_nics(file, &nicList);
   if (Config_getConnUseRDMA(App_getConfig(app)))
   {
      seq_printf(file, "Outbound RDMA Interfaces:\n");
      if (NicAddressList_length(&rdmaNicList) == 0)
         seq_printf(file, "%sAny\n", NIC_INDENTATION_STR);
      else
         ProcFsHelper_readV2_nics(file, &rdmaNicList);
   }

   ListTk_kfreeNicAddressListElems(&nicList);
   NicAddressList_uninit(&nicList);
   ListTk_kfreeNicAddressListElems(&rdmaNicList);
   NicAddressList_uninit(&rdmaNicList);

   return 0;
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

int ProcFsHelper_readV2_connRetriesEnabled(struct seq_file* file, App* app)
{
   bool retriesEnabled = App_getConnRetriesEnabled(app);

   seq_printf(file, "%d\n", retriesEnabled ? 1 : 0);

   return 0;
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

const char* ProcFsHelper_getSessionID(App* app)
{

   Node* localNode = App_getLocalNode(app);
   char* localNodeID = Node_getID(localNode);

   return localNodeID;
}
