#include <app/App.h>
#include <app/log/Logger.h>
#include <common/storage/Metadata.h>
#include <filesystem/FhgfsOpsHelper.h>
#include <app/config/Config.h>
#include <common/net/message/storage/lookup/LookupIntentMsg.h>
#include <common/net/message/storage/lookup/LookupIntentRespMsg.h>
#include <common/net/message/storage/creating/MkDirMsg.h>
#include <common/net/message/storage/creating/MkDirRespMsg.h>
#include <common/net/message/storage/creating/MkFileMsg.h>
#include <common/net/message/storage/creating/MkFileRespMsg.h>
#include <common/net/message/storage/creating/RmDirMsg.h>
#include <common/net/message/storage/creating/RmDirRespMsg.h>
#include <common/net/message/storage/creating/HardlinkMsg.h>
#include <common/net/message/storage/creating/HardlinkRespMsg.h>
#include <common/net/message/storage/creating/UnlinkFileMsg.h>
#include <common/net/message/storage/creating/UnlinkFileRespMsg.h>
#include <common/net/message/storage/listing/ListDirFromOffsetMsg.h>
#include <common/net/message/storage/listing/ListDirFromOffsetRespMsg.h>
#include <common/net/message/storage/moving/RenameMsg.h>
#include <common/net/message/storage/moving/RenameRespMsg.h>
#include <common/net/message/storage/attribs/ListXAttrMsg.h>
#include <common/net/message/storage/attribs/ListXAttrRespMsg.h>
#include <common/net/message/storage/attribs/GetXAttrMsg.h>
#include <common/net/message/storage/attribs/GetXAttrRespMsg.h>
#include <common/net/message/storage/attribs/RemoveXAttrMsg.h>
#include <common/net/message/storage/attribs/RemoveXAttrRespMsg.h>
#include <common/net/message/storage/attribs/SetXAttrMsg.h>
#include <common/net/message/storage/attribs/SetXAttrRespMsg.h>
#include <common/net/message/storage/attribs/RefreshEntryInfoMsg.h>
#include <common/net/message/storage/attribs/RefreshEntryInfoRespMsg.h>
#include <common/net/message/storage/attribs/SetAttrMsg.h>
#include <common/net/message/storage/attribs/SetAttrRespMsg.h>
#include <common/net/message/storage/attribs/StatMsg.h>
#include <common/net/message/storage/attribs/StatRespMsg.h>
#include <common/net/message/storage/TruncFileMsg.h>
#include <common/net/message/storage/TruncFileRespMsg.h>
#include <common/net/message/session/BumpFileVersion.h>
#include <common/net/message/session/BumpFileVersionResp.h>
#include <common/net/message/session/GetFileVersionMsg.h>
#include <common/net/message/session/GetFileVersionRespMsg.h>
#include <common/net/message/session/locking/FLockAppendMsg.h>
#include <common/net/message/session/locking/FLockAppendRespMsg.h>
#include <common/net/message/session/locking/FLockEntryMsg.h>
#include <common/net/message/session/locking/FLockEntryRespMsg.h>
#include <common/net/message/session/locking/FLockRangeMsg.h>
#include <common/net/message/session/locking/FLockRangeRespMsg.h>
#include <common/net/message/session/opening/OpenFileMsg.h>
#include <common/net/message/session/opening/OpenFileRespMsg.h>
#include <common/net/message/session/opening/CloseFileMsg.h>
#include <common/net/message/session/opening/CloseFileRespMsg.h>
#include <common/storage/Path.h>
#include <common/storage/StorageDefinitions.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/SynchronizedCounter.h>
#include <nodes/NodeStoreEx.h>
#include <common/net/message/NetMessage.h>
#include <common/nodes/Node.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/MathTk.h>
#include <common/toolkit/MetadataTk.h>
#include <common/FhgfsTypes.h>
#include <common/toolkit/ackstore/AcknowledgmentStore.h>
#include <common/Common.h>
#include <net/filesystem/FhgfsOpsRemoting.h>
#include <toolkit/NoAllocBufferStore.h>
#include <toolkit/FhgfsPage.h>
#include <filesystem/FhgfsOpsPages.h>
#include "FhgfsOpsCommKit.h"
#include "FhgfsOpsCommKitVec.h"


static inline const char* __FhgfsOpsRemoting_rwTypeToString(enum Fhgfs_RWType);

static bool __FhgfsOpsRemoting_writefileVerify(App* app, RemotingIOInfo* ioInfo,
   struct list_head* states, ssize_t* outWritten, unsigned firstTargetIndex,
   unsigned numStripeNodes);
static inline int64_t __FhgfsOpsRemoting_getChunkOffset(int64_t pos, unsigned chunkSize,
   size_t numNodes, size_t stripeNodeIndex);


struct Fhgfs_RWTypeStrEntry
{
      enum Fhgfs_RWType type;
      const char* typeStr;
};

struct Fhgfs_RWTypeStrEntry const __Fhgfs_RWTypeList[] =
{
   {BEEGFS_RWTYPE_READ,  "read vec"},
   {BEEGFS_RWTYPE_WRITE, "write vec"},
};

#define __FHGFSOPS_REMOTING_RW_SIZE \
   ( (sizeof(__Fhgfs_RWTypeList) ) / (sizeof(struct Fhgfs_RWTypeStrEntry) ) )

#define __FHGFSOPS_REMOTING_msgBufCacheName BEEGFS_MODULE_NAME_STR "-pageVecMsgBufs"
#define __FHGFSOPS_REMOTING_msgBufPoolSize   8 // number of reserve (pre-allocated) msgBufs

const ssize_t __FHGFSOPS_REMOTING_MAX_XATTR_VALUE_SIZE = 60*1000;
const ssize_t __FHGFSOPS_REMOTING_MAX_XATTR_NAME_SIZE = 245;

static struct kmem_cache* FhgfsOpsRemoting_msgBufCache = NULL;
static mempool_t*         FhgfsOpsRemoting_msgBufPool = NULL;

static mempool_t* writefileStatePool;

bool FhgfsOpsRemoting_initMsgBufCache(void)
{
   FhgfsOpsRemoting_msgBufCache = OsCompat_initKmemCache(__FHGFSOPS_REMOTING_msgBufCacheName,
      BEEGFS_COMMKIT_MSGBUF_SIZE, NULL);

   if(!FhgfsOpsRemoting_msgBufCache)
      goto fail_msgBufCache;

   FhgfsOpsRemoting_msgBufPool = mempool_create_slab_pool(__FHGFSOPS_REMOTING_msgBufPoolSize,
      FhgfsOpsRemoting_msgBufCache);

   if(!FhgfsOpsRemoting_msgBufPool)
      goto fail_msgBufPool;

   writefileStatePool = mempool_create_kmalloc_pool(4, sizeof(struct WritefileVecState) );
   if(!writefileStatePool)
      goto fail_statePool;

   return true;

fail_statePool:
   mempool_destroy(FhgfsOpsRemoting_msgBufPool);
fail_msgBufPool:
   kmem_cache_destroy(FhgfsOpsRemoting_msgBufCache);
fail_msgBufCache:
   return false;
}

void FhgfsOpsRemoting_destroyMsgBufCache(void)
{
   if(writefileStatePool)
      mempool_destroy(writefileStatePool);

   if(FhgfsOpsRemoting_msgBufPool)
      mempool_destroy(FhgfsOpsRemoting_msgBufPool);

   if(FhgfsOpsRemoting_msgBufCache)
      kmem_cache_destroy(FhgfsOpsRemoting_msgBufCache);
}


/**
 * Fhgfs_RWType enum value to human-readable string.
 */
const char* __FhgfsOpsRemoting_rwTypeToString(enum Fhgfs_RWType enumType)
{
   size_t type = (size_t)enumType;

   if (likely(type < __FHGFSOPS_REMOTING_RW_SIZE) )
      return __Fhgfs_RWTypeList[type].typeStr;


   printk_fhgfs(KERN_WARNING, "Unknown rwType given to %s(): %d; (dumping stack...)\n",
      __func__, (int) type);

   dump_stack();

   return "Unknown error code";

}

static struct RRPeer rrpeer_from_entryinfo(const EntryInfo* entryInfo)
{
   return EntryInfo_getIsBuddyMirrored(entryInfo)
      ? RRPeer_group(entryInfo->owner.group)
      : RRPeer_target(entryInfo->owner.node);
}

/**
 * Note: Clears FsDirInfo names/types, then sets server offset, adds names/types, sets contents
 * offset and endOfDir in case of RPC success.
 * Note: Also retrieves the optional entry types.
 *
 * @param dirInfo used as input (offsets) and output (contents etc.) parameter
 */
FhgfsOpsErr FhgfsOpsRemoting_listdirFromOffset(const EntryInfo* entryInfo, FsDirInfo* dirInfo,
   unsigned maxOutNames)
{
   FsObjectInfo* fsObjectInfo = (FsObjectInfo*)dirInfo;
   App* app = FsObjectInfo_getApp(fsObjectInfo);

   Logger* log = App_getLogger(app);
   const char* logContext = "Remoting (list dir from offset)";

   int64_t serverOffset = FsDirInfo_getServerOffset(dirInfo);
   ListDirFromOffsetMsg requestMsg;
   RequestResponseNode rrNode = {
      .peer = rrpeer_from_entryinfo(entryInfo),
      .nodeStore = app->metaNodes,
      .targetStates = app->metaStateStore,
      .mirrorBuddies = app->metaBuddyGroupMapper
   };
   RequestResponseArgs rrArgs;
   FhgfsOpsErr requestRes;
   ListDirFromOffsetRespMsg* listDirResp;
   FhgfsOpsErr retVal;

   // prepare request
   ListDirFromOffsetMsg_initFromEntryInfo(
      &requestMsg, entryInfo, serverOffset, maxOutNames, false);

   RequestResponseArgs_prepare(&rrArgs, NULL, (NetMessage*)&requestMsg,
      NETMSGTYPE_ListDirFromOffsetResp);

   // communicate
   requestRes = MessagingTk_requestResponseNodeRetryAutoIntr(app, &rrNode, &rrArgs);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   { // clean-up
      retVal = requestRes;
      goto cleanup_request;
   }

   // handle result
   listDirResp = (ListDirFromOffsetRespMsg*)rrArgs.outRespMsg;
   retVal = (FhgfsOpsErr)ListDirFromOffsetRespMsg_getResult(listDirResp);

   if(likely(retVal == FhgfsOpsErr_SUCCESS) )
   {
      UInt8Vec* dirContentsTypes = FsDirInfo_getDirContentsTypes(dirInfo);
      Int64CpyVec* serverOffsets = FsDirInfo_getServerOffsets(dirInfo);
      StrCpyVec* dirContents = FsDirInfo_getDirContents(dirInfo);
      StrCpyVec* dirContentIDs = FsDirInfo_getEntryIDs(dirInfo);
      bool endOfDirReached;

      FsDirInfo_setCurrentContentsPos(dirInfo, 0);

      Int64CpyVec_clear(serverOffsets);
      UInt8Vec_clear(dirContentsTypes);
      StrCpyVec_clear(dirContents);
      StrCpyVec_clear(dirContentIDs);

      ListDirFromOffsetRespMsg_parseEntryTypes(listDirResp, dirContentsTypes);
      ListDirFromOffsetRespMsg_parseNames(listDirResp, dirContents);
      ListDirFromOffsetRespMsg_parseEntryIDs(listDirResp, dirContentIDs);
      ListDirFromOffsetRespMsg_parseServerOffsets(listDirResp, serverOffsets);

      // check for equal vector lengths
      if(unlikely(
         (UInt8Vec_length(dirContentsTypes) != StrCpyVec_length(dirContents) ) ||
         (UInt8Vec_length(dirContentsTypes) != StrCpyVec_length(dirContentIDs) ) ||
         (UInt8Vec_length(dirContentsTypes) != Int64CpyVec_length(serverOffsets) ) ) )
      { // appearently, at least one of the vector allocations failed
         printk_fhgfs(KERN_WARNING,
            "Memory allocation for directory contents retrieval failed.\n");

         Int64CpyVec_clear(serverOffsets);
         UInt8Vec_clear(dirContentsTypes);
         StrCpyVec_clear(dirContents);
         StrCpyVec_clear(dirContentIDs);

         retVal = FhgfsOpsErr_OUTOFMEM;

         goto cleanup_resp_buffers;
      }

      FsDirInfo_setServerOffset(dirInfo, ListDirFromOffsetRespMsg_getNewServerOffset(listDirResp) );

      endOfDirReached = (StrCpyVec_length(dirContents) < maxOutNames);
      FsDirInfo_setEndOfDir(dirInfo, endOfDirReached);
   }
   else
   {
      int logLevel = (retVal == FhgfsOpsErr_PATHNOTEXISTS) ? Log_DEBUG : Log_NOTICE;

      Logger_logFormatted(log, logLevel, logContext, "ListDirResp error code: %s",
         FhgfsOpsErr_toErrString(retVal) );
   }

   // clean-up

cleanup_resp_buffers:
   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return retVal;
}

/**
 * Resolve path to entry owner and stat the entry.
 * Note: This function should *only* be called to stat the root path, as it is the only dir
 * without a parent, for other entries use _statDirect() instead, as the owner info is already
 * available.
 */
FhgfsOpsErr FhgfsOpsRemoting_statRoot(App* app, fhgfs_stat* outFhgfsStat)
{
   FhgfsOpsErr retVal;

   Node* node;
   const char* logContext = "Stat root dir";
   Logger* log = App_getLogger(app);
   NodeOrGroup rootID;

   NodeStoreEx* nodes = App_getMetaNodes(app);

   node = NodeStoreEx_referenceRootNode(nodes, &rootID);
   if(likely(node) )
   {
      const char* parentEntryID = "";
      const char* entryID       = META_ROOTDIR_ID_STR;
      const char* fileName      = "";
      DirEntryType entryType    = DirEntryType_DIRECTORY;

      EntryInfo entryInfo;
      EntryInfo_init(&entryInfo, rootID, parentEntryID, entryID, fileName, entryType, 0);

      retVal = FhgfsOpsRemoting_statDirect(app, &entryInfo, outFhgfsStat);

      // clean-up
      Node_put(node);
   }
   else
   {
      Logger_logErr(log, logContext, "Unable to proceed without a working root metadata node");
      retVal = FhgfsOpsErr_UNKNOWNNODE;
   }

   return retVal;
}

/**
 * Stat directly from entryInfo.
 *
 * Note: typically you will rather want to call the wrapper FhgfsOpsRemoting_statDirect() instead
 * of this method.
 *
 * @param parentNodeID may be NULL if the caller is not interested (default)
 * @param parentEntryID may be NULL if the caller is not interested (default)
 */
FhgfsOpsErr FhgfsOpsRemoting_statAndGetParentInfo(App* app, const EntryInfo* entryInfo,
   fhgfs_stat* outFhgfsStat, NumNodeID* outParentNodeID, char** outParentEntryID)
{
   Logger* log = App_getLogger(app);
   const char* logContext = "Remoting (stat)";

   StatMsg requestMsg;
   RequestResponseNode rrNode = {
      .peer = rrpeer_from_entryinfo(entryInfo),
      .nodeStore = app->metaNodes,
      .targetStates = app->metaStateStore,
      .mirrorBuddies = app->metaBuddyGroupMapper
   };
   RequestResponseArgs rrArgs;
   FhgfsOpsErr requestRes;
   StatRespMsg* statResp;
   FhgfsOpsErr retVal;

   // prepare request
   StatMsg_initFromEntryInfo(&requestMsg, entryInfo );

   if (outParentNodeID)
      StatMsg_addParentInfoRequest(&requestMsg);

   RequestResponseArgs_prepare(&rrArgs, NULL, (NetMessage*)&requestMsg, NETMSGTYPE_StatResp);

   // communicate
   requestRes = MessagingTk_requestResponseNodeRetryAutoIntr(app, &rrNode, &rrArgs);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   { // clean-up
      retVal = requestRes;
      goto cleanup_request;
   }

   // handle result
   statResp = (StatRespMsg*)rrArgs.outRespMsg;
   retVal = (FhgfsOpsErr)StatRespMsg_getResult(statResp);

   if(retVal == FhgfsOpsErr_SUCCESS)
   {
      StatData* statData = StatRespMsg_getStatData(statResp);
      StatData_getOsStat(statData, outFhgfsStat);

      if (outParentNodeID)
      {
         StatRespMsg_getParentInfo(statResp, outParentNodeID, outParentEntryID);
      }
   }
   else
   {
      LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext, "StatResp error code: %s",
         FhgfsOpsErr_toErrString(retVal) );
      IGNORE_UNUSED_VARIABLE(log);
      IGNORE_UNUSED_VARIABLE(logContext);
   }


   // clean-up
   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return retVal;
}

/**
 * Modify file or dir attributes.
 */
FhgfsOpsErr FhgfsOpsRemoting_setAttr(App* app, const EntryInfo* entryInfo,
   SettableFileAttribs* fhgfsAttr, int validAttribs, const struct FileEvent* event)
{
   Logger* log = App_getLogger(app);
   Config* cfg = App_getConfig(app);
   const char* logContext = "Remoting (set attr)";

   SetAttrMsg requestMsg;
   RequestResponseNode rrNode = {
      .peer = rrpeer_from_entryinfo(entryInfo),
      .nodeStore = app->metaNodes,
      .targetStates = app->metaStateStore,
      .mirrorBuddies = app->metaBuddyGroupMapper
   };
   RequestResponseArgs rrArgs;
   FhgfsOpsErr requestRes;
   SetAttrRespMsg* setAttrResp;
   FhgfsOpsErr retVal;

   // logging
   if(validAttribs & SETATTR_CHANGE_MODE)
      { LOG_DEBUG(log, Log_DEBUG, logContext, "Changing mode"); }

   if(validAttribs & SETATTR_CHANGE_USERID)
      { LOG_DEBUG(log, Log_DEBUG, logContext, "Changing userID"); }

   if(validAttribs & SETATTR_CHANGE_GROUPID)
      { LOG_DEBUG(log, Log_DEBUG, logContext, "Changing groupID"); }

   if(validAttribs & SETATTR_CHANGE_MODIFICATIONTIME)
      { LOG_DEBUG(log, Log_DEBUG, logContext, "Changing modificationTime"); }

   if(validAttribs & SETATTR_CHANGE_LASTACCESSTIME)
      { LOG_DEBUG(log, Log_DEBUG, logContext, "Changing lastAccessTime"); }

   // prepare request
   SetAttrMsg_initFromEntryInfo(&requestMsg, entryInfo, validAttribs, fhgfsAttr, event);

   if (validAttribs & (SETATTR_CHANGE_USERID | SETATTR_CHANGE_GROUPID))
   {
      if(Config_getQuotaEnabled(cfg) )
         NetMessage_addMsgHeaderFeatureFlag((NetMessage*)&requestMsg, SETATTRMSG_FLAG_USE_QUOTA);
   }

   RequestResponseArgs_prepare(&rrArgs, NULL, (NetMessage*)&requestMsg, NETMSGTYPE_SetAttrResp);

   // communicate
   requestRes = MessagingTk_requestResponseNodeRetryAutoIntr(app, &rrNode, &rrArgs);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   { // clean-up
      retVal = requestRes;
      goto cleanup_request;
   }

   // handle result
   setAttrResp = (SetAttrRespMsg*)rrArgs.outRespMsg;
   retVal = (FhgfsOpsErr)SetAttrRespMsg_getValue(setAttrResp);

   if(unlikely(retVal != FhgfsOpsErr_SUCCESS) )
   { // error on server
      int logLevel = (retVal == FhgfsOpsErr_PATHNOTEXISTS) ? Log_DEBUG : Log_NOTICE;

      Logger_logFormatted(log, logLevel, logContext, "SetAttrResp error code: %s",
         FhgfsOpsErr_toErrString(retVal) );
   }


   // clean-up
   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return retVal;
}

/**
 * @param outEntryInfo attribs set only in case of success (and must then be kfreed by the
 * caller)
 */
FhgfsOpsErr FhgfsOpsRemoting_mkdir(App* app, const EntryInfo* parentInfo,
   struct CreateInfo* createInfo, EntryInfo* outEntryInfo)
{
   Logger* log = App_getLogger(app);
   const char* logContext = "Remoting (mkdir)";

   MkDirMsg requestMsg;
   RequestResponseNode rrNode = {
      .peer = rrpeer_from_entryinfo(parentInfo),
      .nodeStore = app->metaNodes,
      .targetStates = app->metaStateStore,
      .mirrorBuddies = app->metaBuddyGroupMapper
   };
   RequestResponseArgs rrArgs;
   FhgfsOpsErr requestRes;
   MkDirRespMsg* mkResp;
   FhgfsOpsErr retVal;

   // prepare request
   MkDirMsg_initFromEntryInfo(&requestMsg, parentInfo, createInfo);

   RequestResponseArgs_prepare(&rrArgs, NULL, (NetMessage*)&requestMsg, NETMSGTYPE_MkDirResp);

   // communicate
   requestRes = MessagingTk_requestResponseNodeRetryAutoIntr(app, &rrNode, &rrArgs);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   { // clean-up
      retVal = requestRes;
      goto cleanup_request;
   }

   // handle result
   mkResp = (MkDirRespMsg*)rrArgs.outRespMsg;
   retVal = (FhgfsOpsErr)MkDirRespMsg_getResult(mkResp);

   if(retVal == FhgfsOpsErr_SUCCESS)
   { // success
      EntryInfo_dup(MkDirRespMsg_getEntryInfo(mkResp), outEntryInfo );
   }
   else
   {
      int logLevel = Log_NOTICE;

      if(retVal == FhgfsOpsErr_EXISTS)
         logLevel = Log_DEBUG; // don't bother user with non-error messages

      Logger_logFormatted(log, logLevel, logContext,
         "MkDirResp ownerID: %u%s parentID: %s name: %s error code: %s ",
         EntryInfo_getOwner(parentInfo), EntryInfo_getOwnerFlag(parentInfo), parentInfo->entryID,
         createInfo->entryName, FhgfsOpsErr_toErrString(retVal));
   }

   // clean-up
   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return retVal;
}

FhgfsOpsErr FhgfsOpsRemoting_rmdir(App* app, const EntryInfo* parentInfo, const char* entryName,
   const struct FileEvent* event)
{
   Logger* log = App_getLogger(app);
   const char* logContext = "Remoting (rmdir)";

   RmDirMsg requestMsg;
   RequestResponseNode rrNode = {
      .peer = rrpeer_from_entryinfo(parentInfo),
      .nodeStore = app->metaNodes,
      .targetStates = app->metaStateStore,
      .mirrorBuddies = app->metaBuddyGroupMapper
   };
   RequestResponseArgs rrArgs;
   FhgfsOpsErr requestRes;
   RmDirRespMsg* rmResp;
   FhgfsOpsErr retVal;

   // prepare request
   RmDirMsg_initFromEntryInfo(&requestMsg, parentInfo, entryName, event);

   RequestResponseArgs_prepare(&rrArgs, NULL, (NetMessage*)&requestMsg, NETMSGTYPE_RmDirResp);

   // communicate
   requestRes = MessagingTk_requestResponseNodeRetryAutoIntr(app, &rrNode, &rrArgs);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   { // clean-up
      retVal = requestRes;
      goto cleanup_request;
   }

   // handle result
   rmResp = (RmDirRespMsg*)rrArgs.outRespMsg;
   retVal = (FhgfsOpsErr)RmDirRespMsg_getValue(rmResp);

   if(retVal != FhgfsOpsErr_SUCCESS)
   {
      int logLevel = Log_NOTICE;

      if( (retVal == FhgfsOpsErr_NOTEMPTY) || (retVal == FhgfsOpsErr_PATHNOTEXISTS) ||
          (retVal == FhgfsOpsErr_INUSE) )
         logLevel = Log_DEBUG; // don't bother user with non-error messages

      Logger_logFormatted(log, logLevel, logContext, "RmDirResp error code: %s",
         FhgfsOpsErr_toErrString(retVal) );
   }

   // clean-up
   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return retVal;
}

/**
 * @param outEntryInfo attribs allocated/set only in case of success (and must then be kfreed by the
 * caller); may be NULL.
 */
FhgfsOpsErr FhgfsOpsRemoting_mkfile(App* app, const EntryInfo* parentInfo,
   struct CreateInfo* createInfo, EntryInfo* outEntryInfo)
{
   return FhgfsOpsRemoting_mkfileWithStripeHints(app, parentInfo, createInfo, 0, 0, outEntryInfo);
}

/**
 * @param numtargets 0 for directory default.
 * @param chunksize 0 for directory default, must be 64K or multiple of 64K otherwise.
 * @param outEntryInfo attribs allocated/set only in case of success (and must then be kfreed by the
 * caller); may be NULL.
 */
FhgfsOpsErr FhgfsOpsRemoting_mkfileWithStripeHints(App* app, const EntryInfo* parentInfo,
   struct CreateInfo* createInfo, unsigned numtargets, unsigned chunksize, EntryInfo* outEntryInfo)
{
   Logger* log = App_getLogger(app);
   const char* logContext = "Remoting (mkfile)";

   MkFileMsg requestMsg;
   RequestResponseNode rrNode = {
      .peer = rrpeer_from_entryinfo(parentInfo),
      .nodeStore = app->metaNodes,
      .targetStates = app->metaStateStore,
      .mirrorBuddies = app->metaBuddyGroupMapper
   };
   RequestResponseArgs rrArgs;
   FhgfsOpsErr requestRes;
   MkFileRespMsg* mkResp;
   FhgfsOpsErr retVal;

   // prepare request
   MkFileMsg_initFromEntryInfo(&requestMsg, parentInfo, createInfo);

   if(numtargets || chunksize)
      MkFileMsg_setStripeHints(&requestMsg, numtargets, chunksize);

   RequestResponseArgs_prepare(&rrArgs, NULL, (NetMessage*)&requestMsg, NETMSGTYPE_MkFileResp);

   // communicate
   requestRes = MessagingTk_requestResponseNodeRetryAutoIntr(app, &rrNode, &rrArgs);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   { // clean-up
      retVal = requestRes;
      goto cleanup_request;
   }

   // handle result
   mkResp = (MkFileRespMsg*)rrArgs.outRespMsg;
   retVal = (FhgfsOpsErr)MkFileRespMsg_getResult(mkResp);

   if(retVal == FhgfsOpsErr_SUCCESS)
   { // success
      const EntryInfo* entryInfo = MkFileRespMsg_getEntryInfo(mkResp);

      if(outEntryInfo)
         EntryInfo_dup(entryInfo, outEntryInfo);
   }
   else
   {
      int logLevel = Log_NOTICE;

      if(retVal == FhgfsOpsErr_EXISTS)
         logLevel = Log_DEBUG; // don't bother user with non-error messages

      Logger_logFormatted(log, logLevel, logContext,
         "MkFileResp: ownerID: %u%s parentID: %s name: %s error code: %s",
         EntryInfo_getOwner(parentInfo), EntryInfo_getOwnerFlag(parentInfo), parentInfo->entryID,
         createInfo->entryName, FhgfsOpsErr_toErrString(retVal));

   }

   // clean-up
   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return retVal;
}

FhgfsOpsErr FhgfsOpsRemoting_unlinkfile(App* app, const EntryInfo* parentInfo,
   const char* entryName, const struct FileEvent* event)
{
   Logger* log = App_getLogger(app);
   const char* logContext = "Remoting (unlink file)";

   UnlinkFileMsg requestMsg;
   RequestResponseNode rrNode = {
      .peer = rrpeer_from_entryinfo(parentInfo),
      .nodeStore = app->metaNodes,
      .targetStates = app->metaStateStore,
      .mirrorBuddies = app->metaBuddyGroupMapper
   };
   RequestResponseArgs rrArgs;
   FhgfsOpsErr requestRes;
   UnlinkFileRespMsg* unlinkResp;
   FhgfsOpsErr retVal;

   // prepare request
   UnlinkFileMsg_initFromEntryInfo(&requestMsg, parentInfo, entryName, event);

   RequestResponseArgs_prepare(&rrArgs, NULL, (NetMessage*)&requestMsg, NETMSGTYPE_UnlinkFileResp);

   // communicate
   requestRes = MessagingTk_requestResponseNodeRetryAutoIntr(app, &rrNode, &rrArgs);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   { // clean-up
      retVal = requestRes;
      goto cleanup_request;
   }

   // handle result
   unlinkResp = (UnlinkFileRespMsg*)rrArgs.outRespMsg;
   retVal = (FhgfsOpsErr)UnlinkFileRespMsg_getValue(unlinkResp);

   if(retVal != FhgfsOpsErr_SUCCESS)
   {
      int logLevel = Log_NOTICE;

      if(retVal == FhgfsOpsErr_PATHNOTEXISTS)
         logLevel = Log_DEBUG; // don't bother user with non-error messages

      Logger_logFormatted(log, logLevel, logContext, "UnlinkFileResp error code: %s",
         FhgfsOpsErr_toErrString(retVal) );
   }

   // clean-up
   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return retVal;
}

/**
 * @param ioInfo in and out arg; in case of success: fileHandleID and pathInfo will be set
 *        pattern will be set if it is NULL; values have to be freed by the caller
 */
FhgfsOpsErr FhgfsOpsRemoting_openfile(const EntryInfo* entryInfo, RemotingIOInfo* ioInfo,
   uint64_t* outVersion, const struct FileEvent* event)
{
   App* app = ioInfo->app;
   Logger* log = App_getLogger(app);
   Config* cfg = App_getConfig(app);
   const char* logContext = "Remoting (open file)";

   const NumNodeID localNodeNumID = Node_getNumID(App_getLocalNode(app) );
   OpenFileMsg requestMsg;
   RequestResponseNode rrNode = {
      .peer = rrpeer_from_entryinfo(entryInfo),
      .nodeStore = app->metaNodes,
      .targetStates = app->metaStateStore,
      .mirrorBuddies = app->metaBuddyGroupMapper
   };
   RequestResponseArgs rrArgs;
   FhgfsOpsErr requestRes;
   OpenFileRespMsg* openResp;
   FhgfsOpsErr retVal;

   // log open flags
   LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext,
      "EntryID: %s access: %s%s%s; extra flags: %s|%s", entryInfo->entryID,
      ioInfo->accessFlags & OPENFILE_ACCESS_READWRITE ? "rw" : "",
      ioInfo->accessFlags & OPENFILE_ACCESS_WRITE     ? "w" : "",
      ioInfo->accessFlags & OPENFILE_ACCESS_READ      ? "r" : "",
      ioInfo->accessFlags & OPENFILE_ACCESS_APPEND    ? "append" : "",
      ioInfo->accessFlags & OPENFILE_ACCESS_TRUNC     ? "trunc" : "");

   // prepare request msg
   OpenFileMsg_initFromSession(&requestMsg, localNodeNumID, entryInfo, ioInfo->accessFlags,
         event);

   if(Config_getQuotaEnabled(cfg) )
      NetMessage_addMsgHeaderFeatureFlag((NetMessage*)&requestMsg, OPENFILEMSG_FLAG_USE_QUOTA);

   RequestResponseArgs_prepare(&rrArgs, NULL, (NetMessage*)&requestMsg, NETMSGTYPE_OpenFileResp);

   // communicate
   requestRes = MessagingTk_requestResponseNodeRetryAutoIntr(app, &rrNode, &rrArgs);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   { // clean-up
      retVal = requestRes;
      goto cleanup_request;
   }

   // handle result
   openResp = (OpenFileRespMsg*)rrArgs.outRespMsg;
   retVal = (FhgfsOpsErr)OpenFileRespMsg_getResult(openResp);

   if(likely(retVal == FhgfsOpsErr_SUCCESS) )
   { // success => store file details
      const PathInfo* msgPathInfoPtr;

      ioInfo->fileHandleID = StringTk_strDup(OpenFileRespMsg_getFileHandleID(openResp) );

      LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext, "FileHandleID: %s",
         OpenFileRespMsg_getFileHandleID(openResp) );

      if(!ioInfo->pattern)
      { // inode doesn't have pattern yet => create it from response
         StripePattern* pattern = OpenFileRespMsg_createPattern(openResp);

         // check stripe pattern validity
         if(unlikely(StripePattern_getPatternType(pattern) == STRIPEPATTERN_Invalid) )
         { // invalid pattern
            Logger_logErrFormatted(log, logContext,
               "Received invalid stripe pattern from metadata node: %u%s; FileHandleID: %s",
               EntryInfo_getOwner(entryInfo), EntryInfo_getOwnerFlag(entryInfo),
               OpenFileRespMsg_getFileHandleID(openResp) );

            StripePattern_virtualDestruct(pattern);

            // nevertheless, the file is open now on mds => close it
            FhgfsOpsHelper_closefileWithAsyncRetry(entryInfo, ioInfo, NULL);

            kfree(ioInfo->fileHandleID);

            retVal = FhgfsOpsErr_INTERNAL;
         }
         else
            ioInfo->pattern = pattern; // received a valid pattern
      }

      msgPathInfoPtr = OpenFileRespMsg_getPathInfo(openResp);
      PathInfo_update(ioInfo->pathInfo, msgPathInfoPtr);

      if (outVersion)
         *outVersion = openResp->fileVersion;
   }
   else
   { // error on server
      int logLevel = (retVal == FhgfsOpsErr_PATHNOTEXISTS) ? Log_DEBUG : Log_NOTICE;

      Logger_logFormatted(log, logLevel, logContext, "OpenFileResp error code: %s",
         FhgfsOpsErr_toErrString(retVal) );
   }

   // clean-up
   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return retVal;
}

FhgfsOpsErr FhgfsOpsRemoting_closefile(const EntryInfo* entryInfo, RemotingIOInfo* ioInfo,
   const struct FileEvent* event)
{
   return FhgfsOpsRemoting_closefileEx(entryInfo, ioInfo, true, event);
}

/**
 * Note: You probably want to call FhgfsOpsRemoting_closefile() instead of this method.
 *
 * @param allowRetries usually true, only callers like InternodeSyncer might use false
 * here to make sure they don't block on communication retries too long.
 */
FhgfsOpsErr FhgfsOpsRemoting_closefileEx(const EntryInfo* entryInfo, RemotingIOInfo* ioInfo,
   bool allowRetries, const struct FileEvent* event)
{
   App* app = ioInfo->app;
   Config* cfg = App_getConfig(app);

   Logger* log = App_getLogger(app);
   const char* logContext = "Remoting (close file)";

   const NumNodeID localNodeNumID = Node_getNumID(App_getLocalNode(app) );
   const int maxUsedTargetIndex = AtomicInt_read(ioInfo->maxUsedTargetIndex);
   CloseFileMsg requestMsg;
   RequestResponseNode rrNode = {
      .peer = rrpeer_from_entryinfo(entryInfo),
      .nodeStore = app->metaNodes,
      .targetStates = app->metaStateStore,
      .mirrorBuddies = app->metaBuddyGroupMapper
   };
   RequestResponseArgs rrArgs;
   FhgfsOpsErr requestRes;
   CloseFileRespMsg* closeResp;
   FhgfsOpsErr retVal;

   // prepare request
   CloseFileMsg_initFromSession(&requestMsg, localNodeNumID, ioInfo->fileHandleID, entryInfo,
      maxUsedTargetIndex, event);

   if(Config_getTuneEarlyCloseResponse(cfg) ) // (note: linux doesn't return release() res to apps)
      NetMessage_addMsgHeaderFeatureFlag( (NetMessage*)&requestMsg,
         CLOSEFILEMSG_FLAG_EARLYRESPONSE);

   if(ioInfo->needsAppendLockCleanup && *ioInfo->needsAppendLockCleanup)
      NetMessage_addMsgHeaderFeatureFlag( (NetMessage*)&requestMsg,
         CLOSEFILEMSG_FLAG_CANCELAPPENDLOCKS);

   RequestResponseArgs_prepare(&rrArgs, NULL, (NetMessage*)&requestMsg, NETMSGTYPE_CloseFileResp);

   // connect
   if(allowRetries)
   { // normal case
      requestRes = MessagingTk_requestResponseNodeRetryAutoIntr(app, &rrNode, &rrArgs);
   }
   else
   { // caller doesn't want retries => just use the requestResponse method without retries...
      requestRes = MessagingTk_requestResponseNode(app, &rrNode, &rrArgs);
   }

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   { // clean-up
      retVal = requestRes;
      goto cleanup_request;
   }

   // handle result
   closeResp = (CloseFileRespMsg*)rrArgs.outRespMsg;
   retVal = (FhgfsOpsErr)CloseFileRespMsg_getValue(closeResp);

   if(likely(retVal == FhgfsOpsErr_SUCCESS) )
   { // success
      LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext, "FileHandleID: %s", ioInfo->fileHandleID);
   }
   else
   { // error
      Logger_logFormatted(log, Log_NOTICE, logContext, "CloseFileResp error: %s (FileHandleID: %s)",
         FhgfsOpsErr_toErrString(retVal), ioInfo->fileHandleID);
   }

   // clean-up
   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return retVal;
}

/**
 * This is the common code base of _flockEntryEx, _flockRangeEx, _flockAppendEx, which asks for a
 * lock and resends the lock request every few seconds.
 *
 * @param requestMsg initialized lock request msg (will be sent to ownerNodeID), must be cleaned up
 * by caller.
 * @param respMsgType must be derived from SimpleIntMsg, contained result will be interpreted as
 * FhgfsOpsErr_...
 * @param allowRetries usually true, only callers like InternodeSyncer might use false
 * here to make sure they don't block on communication retries too long.
 */
FhgfsOpsErr __FhgfsOpsRemoting_flockGenericEx(NetMessage* requestMsg, unsigned respMsgType,
   NodeOrGroup owner, bool isBuddyMirrored, App* app, const char* fileHandleID, int lockTypeFlags,
   char* lockAckID, bool allowRetries, RWLock* eiRLock)
{
   const int resendIntervalMS = 5000;

   const char* logContext = "Remoting (generic lock)";

   Logger* log = App_getLogger(app);
   AcknowledgmentStore* ackStore = App_getAckStore(app);

   WaitAck lockWaitAck;
   WaitAckMap waitAcks;
   WaitAckMap receivedAcks;
   WaitAckNotification ackNotifier;

   FhgfsOpsErr requestRes;
   SimpleIntMsg* flockResp;
   FhgfsOpsErr retVal;

   IGNORE_UNUSED_DEBUG_VARIABLE(log);
   IGNORE_UNUSED_DEBUG_VARIABLE(logContext);

   // register lock ack wait...

   WaitAck_init(&lockWaitAck, lockAckID, NULL);
   WaitAckMap_init(&waitAcks);
   WaitAckMap_init(&receivedAcks);
   WaitAckNotification_init(&ackNotifier);

   WaitAckMap_insert(&waitAcks, lockAckID, &lockWaitAck);

   AcknowledgmentStore_registerWaitAcks(ackStore, &waitAcks, &receivedAcks, &ackNotifier);

   do
   {
      RequestResponseNode rrNode = {
         .peer = isBuddyMirrored
            ? RRPeer_group(owner.group)
            : RRPeer_target(owner.node),
         .nodeStore = app->metaNodes,
         .targetStates = app->metaStateStore,
         .mirrorBuddies = app->metaBuddyGroupMapper
      };
      RequestResponseArgs rrArgs;

      // prepare request
      RequestResponseArgs_prepare(&rrArgs, NULL, (NetMessage*)requestMsg, respMsgType);

      // connect & communicate
      if(allowRetries)
      { // normal case
         requestRes = MessagingTk_requestResponseNodeRetryAutoIntr(app, &rrNode, &rrArgs);
      }
      else
      { // caller doesn't want retries => just use the requestResponse tool without retries...
         requestRes = MessagingTk_requestResponseNode(app, &rrNode, &rrArgs);
      }

      if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
      { // clean-up
         retVal = requestRes;
         break;
      }

      // store result
      flockResp = (SimpleIntMsg*)rrArgs.outRespMsg;
      retVal = (FhgfsOpsErr)SimpleIntMsg_getValue(flockResp);

      // cleanup response
      // (note: cleanup is done here to not block the buffer while we're waiting for delayed ack)
      RequestResponseArgs_freeRespBuffers(&rrArgs, app);

      // handle result

      if( (retVal == FhgfsOpsErr_WOULDBLOCK) && !(lockTypeFlags & ENTRYLOCKTYPE_NOWAIT) )
      {
         // not immediately granted and caller wants to wait for the lock

         bool waitRes;

         if (eiRLock)
            RWLock_readUnlock(eiRLock);

         waitRes = AcknowledgmentStore_waitForAckCompletion(
            ackStore, &waitAcks, &ackNotifier, resendIntervalMS);

         if (eiRLock)
            RWLock_readLock(eiRLock);

         if(waitRes)
         { // received lock grant
            LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext, "received async lock grant. "
               "(handleID: %s)", fileHandleID);

            retVal = FhgfsOpsErr_SUCCESS;
         }
         else
         if (!waitRes && !signal_pending(current))
         { // no grant received yet => resend request
            LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext, "no grant yet. bailing to caller... "
               "(handleID: %s)", fileHandleID);

            break;
         }
         else
         { // signal pending => cancel waiting
            LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext, "canceling wait due to pending signal "
               "(handleID: %s)", fileHandleID);

            retVal = FhgfsOpsErr_INTERRUPTED;
         }
      }

      if(retVal == FhgfsOpsErr_SUCCESS)
      { // success
         LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext, "received grant (handleID: %s)",
            fileHandleID);
      }
      else
      { // error
         LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext,
            "FLockResp error: %s (FileHandleID: %s)",
            FhgfsOpsErr_toErrString(retVal), fileHandleID);
      }
   } while (0);

   /* note: if we were interrupted after sending a lock request and user allowed waiting, we have
      one last chance now to get our ack after unregistering. (this check is also important to avoid
      race conditions because we still send acks to lock grants as long as we're registered.) */

   AcknowledgmentStore_unregisterWaitAcks(ackStore, &waitAcks);

   /* now that we're unregistered, we can safely check whether we received the grant after the
      interrupt and before we unregistered ourselves (see corresponding note above)  */

   if( (retVal == FhgfsOpsErr_INTERRUPTED) && WaitAckMap_length(&receivedAcks) )
   { // good, we received the grant just in time
      retVal = FhgfsOpsErr_SUCCESS;
   }

   WaitAckNotification_uninit(&ackNotifier);
   WaitAckMap_uninit(&receivedAcks);
   WaitAckMap_uninit(&waitAcks);

   return retVal;
}

/**
 * Note: You probably want to call FhgfsOpsRemoting_flockAppend() instead of this method.
 *
 * @param clientFD will typically be 0, because we request append locks for the whole client (not
 * for individual FDs).
 * @param ownerPID just informative (value is stored on MDS, but not used)
 * @param allowRetries usually true, only callers like InternodeSyncer might use false
 * here to make sure they don't block on communication retries too long.
 */
FhgfsOpsErr FhgfsOpsRemoting_flockAppendEx(const EntryInfo* entryInfo, RWLock* eiRLock, App* app,
   const char* fileHandleID, int64_t clientFD, int ownerPID, int lockTypeFlags, bool allowRetries)
{
   const char* logContext = "Remoting (flock append)";

   Logger* log = App_getLogger(app);
   AtomicInt* atomicCounter = App_getLockAckAtomicCounter(app);
   const NumNodeID localNodeNumID = Node_getNumID(App_getLocalNode(app) );
   const char* localNodeID = Node_getID(App_getLocalNode(app) );
   char* lockAckID;

   FLockAppendMsg requestMsg;
   FhgfsOpsErr lockRes;

   // (note: lockAckID must be _globally_ unique)
   lockAckID = StringTk_kasprintf("%X-%s-alck", AtomicInt_incAndRead(atomicCounter), localNodeID);
   if(unlikely(!lockAckID) )
   { // out of mem
      Logger_logErrFormatted(log, logContext, "Unable to proceed - out of memory");
      return FhgfsOpsErr_INTERNAL;
   }

   do
   {
      FLockAppendMsg_initFromSession(&requestMsg, localNodeNumID, fileHandleID, entryInfo,
         clientFD, ownerPID, lockTypeFlags, lockAckID);

      lockRes = __FhgfsOpsRemoting_flockGenericEx( (NetMessage*)&requestMsg,
         NETMSGTYPE_FLockAppendResp, entryInfo->owner, EntryInfo_getIsBuddyMirrored(entryInfo),
         app, fileHandleID, lockTypeFlags, lockAckID, allowRetries, eiRLock);
   } while (lockRes == FhgfsOpsErr_WOULDBLOCK && !(lockTypeFlags & ENTRYLOCKTYPE_NOWAIT));

   // cleanup
   kfree(lockAckID);

   return lockRes;
}

/**
 * Note: You probably want to call FhgfsOpsRemoting_flockEntry() instead of this method.
 *
 * @param allowRetries usually true, only callers like InternodeSyncer might use false
 * here to make sure they don't block on communication retries too long.
 */
FhgfsOpsErr FhgfsOpsRemoting_flockEntryEx(const EntryInfo* entryInfo, RWLock* eiRLock, App* app,
   const char* fileHandleID, int64_t clientFD, int ownerPID, int lockTypeFlags, bool allowRetries)
{
   const char* logContext = "Remoting (flock entry)";

   Logger* log = App_getLogger(app);
   AtomicInt* atomicCounter = App_getLockAckAtomicCounter(app);
   const NumNodeID localNodeNumID = Node_getNumID(App_getLocalNode(app) );
   const char* localNodeID = Node_getID(App_getLocalNode(app) );
   char* lockAckID;

   FLockEntryMsg requestMsg;
   FhgfsOpsErr lockRes;

   // (note: lockAckID must be _globally_ unique)
   lockAckID = StringTk_kasprintf("%X-%s-elck", AtomicInt_incAndRead(atomicCounter), localNodeID);
   if(unlikely(!lockAckID) )
   { // out of mem
      Logger_logErrFormatted(log, logContext, "Unable to proceed - out of memory");
      return FhgfsOpsErr_INTERNAL;
   }

   do
   {
      FLockEntryMsg_initFromSession(&requestMsg, localNodeNumID, fileHandleID, entryInfo,
         clientFD, ownerPID, lockTypeFlags, lockAckID);

      lockRes = __FhgfsOpsRemoting_flockGenericEx((NetMessage*) &requestMsg,
         NETMSGTYPE_FLockEntryResp, entryInfo->owner, EntryInfo_getIsBuddyMirrored(entryInfo), app,
         fileHandleID, lockTypeFlags, lockAckID, allowRetries, eiRLock);
   } while (lockRes == FhgfsOpsErr_WOULDBLOCK && !(lockTypeFlags & ENTRYLOCKTYPE_NOWAIT));

   // cleanup
   kfree(lockAckID);

   return lockRes;
}

/**
 * Note: You probably want to call FhgfsOpsRemoting_flockRange() instead of this method.
 *
 * @param allowRetries usually true, only callers like InternodeSyncer might use false
 * here to make sure they don't block on communication retries too long.
 */
FhgfsOpsErr FhgfsOpsRemoting_flockRangeEx(const EntryInfo* entryInfo, RWLock* eiRLock,
   App* app, const char* fileHandleID, int ownerPID, int lockTypeFlags, uint64_t start, uint64_t end,
   bool allowRetries)
{
   const char* logContext = "Remoting (flock range)";

   Logger* log = App_getLogger(app);
   AtomicInt* atomicCounter = App_getLockAckAtomicCounter(app);
   const NumNodeID localNodeNumID = Node_getNumID(App_getLocalNode(app) );
   const char* localNodeID = Node_getID(App_getLocalNode(app) );
   char* lockAckID;

   FLockRangeMsg requestMsg;
   FhgfsOpsErr lockRes;

   // (note: lockAckID must be _globally_ unique)
   lockAckID = StringTk_kasprintf("%X-%s-rlck", AtomicInt_incAndRead(atomicCounter), localNodeID);
   if(unlikely(!lockAckID) )
   { // out of mem
      Logger_logErrFormatted(log, logContext, "Unable to proceed - out of memory");
      return FhgfsOpsErr_INTERNAL;
   }

   do
   {
      FLockRangeMsg_initFromSession(&requestMsg, localNodeNumID, fileHandleID, entryInfo,
         ownerPID, lockTypeFlags, start, end, lockAckID);

      lockRes = __FhgfsOpsRemoting_flockGenericEx((NetMessage*) &requestMsg,
         NETMSGTYPE_FLockRangeResp, entryInfo->owner, EntryInfo_getIsBuddyMirrored(entryInfo), app,
         fileHandleID, lockTypeFlags, lockAckID, allowRetries, eiRLock);
   } while (lockRes == FhgfsOpsErr_WOULDBLOCK && !(lockTypeFlags & ENTRYLOCKTYPE_NOWAIT));

   // cleanup
   kfree(lockAckID);

   return lockRes;
}

/**
 * Check if there have been errors during _writefile().
 *
 * @supposedWritten  - number of bytes already written, including the current step; if an error
 *                     came up, the number of unwritten bytes of this step will be subtracted.
 * @outWritten       - number of successfully written bytes or negative fhgfs error code.
 * @firstWriteDone   - bit mask of the targets; the bit of a target is true if a chunk was
 *                     successful written to this target (for server cache loss detection).
 * @firstTargetIndex - the index of the first stripe target (i.e. target index of states[0]).
 * @numStripeNodes   - count of stripe targets.
 * @return           - true if all was fine, false if there was an error.
 */
static bool __FhgfsOpsRemoting_writefileVerify(App* app, RemotingIOInfo* ioInfo,
   struct list_head* states, ssize_t* outWritten, unsigned firstTargetIndex,
   unsigned numStripeNodes)
{
   Logger* log = App_getLogger(app);
   const char* logContext = "Remoting (write file)";

   unsigned i = 0;
   struct WritefileVecState* vstate;

   *outWritten = 0;

   list_for_each_entry(vstate, states, base.base.targetInfoList)
   {
      WritefileState* state = &vstate->base;
      /* note: we abort on the first error that comes up to return the number of successfully
         written bytes till this error. (with a stripe count > 1 there might be data successfully
         written to following targets, but this info cannot be returned to the user.) */

      if(likely(state->expectedNodeResult >= state->base.nodeResult) )
      {
         // update BitStore with first write done
         unsigned currentTargetIndex = (firstTargetIndex + i) % numStripeNodes;

         BitStore_setBit(ioInfo->firstWriteDone, currentTargetIndex, true);
         *outWritten += state->base.nodeResult;
         i += 1;
      }

      if(likely(state->expectedNodeResult == state->base.nodeResult) )
         continue;

      // result was other than we expected.

      if(state->base.nodeResult >= 0)
      { // node wrote only a part of the data (probably disk full => makes no sense to go on)
         Logger_logFormatted(log, Log_NOTICE, logContext,
            "Problem storage targetID: %hu; "
            "Msg: Node wrote only %lld bytes (expected %lld bytes); FileHandle: %s",
            state->base.targetID, state->base.nodeResult, state->expectedNodeResult,
            ioInfo->fileHandleID);
      }
      else
      { // error occurred
         FhgfsOpsErr nodeError = -(state->base.nodeResult);

         if(nodeError == FhgfsOpsErr_INTERRUPTED) // this is normal on ctrl+c (=> no logErr() )
            Logger_logFormatted(log, Log_DEBUG, logContext,
               "Storage targetID: %hu; Msg: %s; FileHandle: %s",
               state->base.targetID, FhgfsOpsErr_toErrString(nodeError), ioInfo->fileHandleID);
         else
            Logger_logErrFormatted(log, logContext,
               "Error storage targetID: %hu; Msg: %s; FileHandle: %s",
               state->base.targetID, FhgfsOpsErr_toErrString(nodeError), ioInfo->fileHandleID);

         *outWritten = state->base.nodeResult;
      }

      return false;
   } // end of for-loop (result verification)

   return true;
}

ssize_t FhgfsOpsRemoting_writefile(const char __user *buf, size_t size, loff_t offset,
   RemotingIOInfo* ioInfo)
{
   struct iovec iov = {
      .iov_base = (char*) buf,
      .iov_len = size,
   };
   struct iov_iter iter;

   BEEGFS_IOV_ITER_INIT(&iter, WRITE, &iov, 1, size);

   return FhgfsOpsRemoting_writefileVec(&iter, offset, ioInfo, false);
}

static void writefile_nextIter(CommKitContext* context, WritefileState* state)
{
   struct WritefileVecState* vecState = container_of(state, struct WritefileVecState, base);

   state->data = vecState->data;
}

/**
 * Single-threaded parallel file write.
 * Works for mirrored and unmirrored files. In case of a mirrored file, the mirror data will be
 * forwarded by the servers.
 *
 * @return number of bytes written or negative fhgfs error code
 */
ssize_t FhgfsOpsRemoting_writefileVec(const struct iov_iter* iter, loff_t offset,
   RemotingIOInfo* ioInfo, bool serializeWrites)
{
   App* app = ioInfo->app;

   bool verifyRes;
   ssize_t verifyValue;
   ssize_t retVal = iter->count;
   size_t toBeWritten = iter->count;
   loff_t currentOffset = offset;
   struct iov_iter iterCopy = *iter;

   StripePattern* pattern = ioInfo->pattern;
   unsigned chunkSize = StripePattern_getChunkSize(pattern);
   UInt16Vec* targetIDs = pattern->getStripeTargetIDs(pattern);
   unsigned numStripeTargets = UInt16Vec_length(targetIDs);
   int maxUsedTargetIndex = AtomicInt_read(ioInfo->maxUsedTargetIndex);

   __FhgfsOpsRemoting_logDebugIOCall(__func__, iter->count, offset, ioInfo, NULL);

   while(toBeWritten)
   {
      unsigned currentTargetIndex = pattern->getStripeTargetIndex(pattern, currentOffset);
      unsigned firstTargetIndex = currentTargetIndex;
      unsigned numWorks = 0;
      size_t expectedWritten = 0;
      LIST_HEAD(statesList);

      /* stripeset-loop: loop over one stripe set (using dynamically determined stripe target
         indices). */
      while(toBeWritten && (numWorks < numStripeTargets) )
      {
         size_t currentChunkSize =
            StripePattern_getChunkEnd(pattern, currentOffset) - currentOffset + 1;
         loff_t currentNodeLocalOffset = __FhgfsOpsRemoting_getChunkOffset(
            currentOffset, chunkSize, numStripeTargets, currentTargetIndex);
         struct iov_iter chunkIter;

         struct WritefileVecState* state = mempool_alloc(writefileStatePool,
            list_empty(&statesList) ? GFP_NOFS : GFP_NOWAIT);

         if (!state)
            break;

         maxUsedTargetIndex = MAX(maxUsedTargetIndex, (int)currentTargetIndex);

         chunkIter = iterCopy;
         iov_iter_truncate(&chunkIter, currentChunkSize);

         // prepare the state information
         FhgfsOpsCommKit_initWritefileState(
            &state->base,
            currentNodeLocalOffset,
            chunkIter.count,
            UInt16Vec_at(targetIDs, currentTargetIndex) );

         state->base.firstWriteDoneForTarget =
            BitStore_getBit(ioInfo->firstWriteDone, currentTargetIndex);

         state->data = chunkIter;

         list_add_tail(&state->base.base.targetInfoList, &statesList);

         // (note on buddy mirroring: server-side mirroring is default, so nothing to do here)

         App_incNumRemoteWrites(app);

         // prepare for next loop
         currentOffset += chunkIter.count;
         toBeWritten -= chunkIter.count;
         expectedWritten += chunkIter.count;
         numWorks++;
         iov_iter_advance(&iterCopy, chunkIter.count);
         currentTargetIndex = (currentTargetIndex + 1) % numStripeTargets;

         if(serializeWrites)
            break;
      }

      // communicate with the nodes
      FhgfsOpsCommKit_writefileV2bCommunicate(app, ioInfo, &statesList, writefile_nextIter, NULL);

      // verify results
      verifyRes = __FhgfsOpsRemoting_writefileVerify(app, ioInfo, &statesList, &verifyValue,
         firstTargetIndex, numStripeTargets);

      while (!list_empty(&statesList) )
      {
         struct WritefileVecState* state = list_first_entry(&statesList, struct WritefileVecState,
            base.base.targetInfoList);

         list_del(&state->base.base.targetInfoList);
         mempool_free(state, writefileStatePool);
      }

      if(unlikely(!verifyRes) )
      {
         retVal = verifyValue;
         break;
      }
   } // end of while-loop (write out data)

   AtomicInt_max(ioInfo->maxUsedTargetIndex, maxUsedTargetIndex);

   return retVal;
}

/**
 * Write/read a vector (array) of pages to/from the storage servers.
 *
 * @return number of bytes written or negative fhgfs error code
 */
ssize_t FhgfsOpsRemoting_rwChunkPageVec(FhgfsChunkPageVec *pageVec, RemotingIOInfo* ioInfo,
   Fhgfs_RWType rwType)
{
   App* app = ioInfo->app;
   Logger* log = App_getLogger(app);

   bool needReadWriteHandlePages = false; // needs to be done on error in this method

   loff_t offset = FhgfsChunkPageVec_getFirstPageFileOffset(pageVec);

   const char* logContext = "Remoting read/write vec";
   const char* rwTypeStr  = __FhgfsOpsRemoting_rwTypeToString(rwType);

   StripePattern* pattern = ioInfo->pattern;
   unsigned chunkSize = StripePattern_getChunkSize(pattern);
   size_t chunkPages = RemotingIOInfo_getNumPagesPerStripe(ioInfo);
   unsigned numPages = FhgfsChunkPageVec_getSize(pageVec);
   unsigned pageIdx = 0; // page index

   UInt16Vec* targetIDs = pattern->getStripeTargetIDs(pattern);
   unsigned numStripeNodes = UInt16Vec_length(targetIDs);
   int maxUsedTargetIndex = AtomicInt_read(ioInfo->maxUsedTargetIndex);

   unsigned targetIndex = pattern->getStripeTargetIndex(pattern, offset);

   loff_t chunkOffset = __FhgfsOpsRemoting_getChunkOffset(
      offset, chunkSize, numStripeNodes, targetIndex);

   ssize_t retVal;

   char* msgBuf;
   FhgfsCommKitVec state;

   // sanity check
   if (numPages > chunkPages)
   {
      Logger_logErrFormatted(log, logContext, "Bug: %s: numPages (%u) > numChunkPages (%lu)!",
         rwTypeStr, numPages, chunkPages);

      needReadWriteHandlePages = true;
      retVal = -EINVAL;
      goto out;
   }

#ifdef LOG_DEBUG_MESSAGES
   {
      ssize_t supposedSize = FhgfsChunkPageVec_getDataSize(pageVec);
      __FhgfsOpsRemoting_logDebugIOCall(__func__, supposedSize, offset, ioInfo, rwTypeStr);
   }
#endif

   msgBuf = mempool_alloc(FhgfsOpsRemoting_msgBufPool, GFP_NOFS);
   if (unlikely(!msgBuf) )
   {  // pool allocs must not fail!
      printk_fhgfs(KERN_WARNING, "kernel bug (%s): mempool_alloc failed\n", __func__);

      needReadWriteHandlePages = true;
      retVal = -ENOMEM;
      goto outNoAlloc;
   }

   /* in debug mode we use individual allocations instead of a single big buffer, as we can detect
      array out of bounds for those (if the proper kernel options are set) */

   maxUsedTargetIndex = MAX(maxUsedTargetIndex, (int)targetIndex);

   // prepare the state information
   state = FhgfsOpsCommKitVec_assignRWfileState(
      pageVec, pageIdx, numPages, chunkOffset, UInt16Vec_at(targetIDs, targetIndex), msgBuf);

   FhgfsOpsCommKitVec_setRWFileStateFirstWriteDone(
      BitStore_getBit(ioInfo->firstWriteDone, targetIndex), &state);

   if (rwType == BEEGFS_RWTYPE_WRITE)
   {
      // (note on buddy mirroring: server-side mirroring is default, so nothing to do here)

      App_incNumRemoteWrites(app);
   }
   else
   {
      App_incNumRemoteReads(app);
   }

   retVal = FhgfsOpsCommKitVec_rwFileCommunicate(app, ioInfo, &state, rwType);

   if(retVal > 0)
   {
      if (rwType == BEEGFS_RWTYPE_WRITE)
         BitStore_setBit(ioInfo->firstWriteDone, targetIndex, true);
   }
   else if (retVal == -FhgfsOpsErr_COMMUNICATION)
   {
      // commkit has done no communication at all, and thus hasn't touched any pages. it is our
      // responsibility to end pending io with an error now.
      needReadWriteHandlePages = true;
   }

   LOG_DEBUG_FORMATTED(log, Log_SPAM, logContext, "fileHandleID: %s rwType %s: sum-result %ld",
      ioInfo->fileHandleID, rwTypeStr, retVal);
   IGNORE_UNUSED_VARIABLE(log);
   IGNORE_UNUSED_VARIABLE(logContext);
   IGNORE_UNUSED_VARIABLE(rwTypeStr);

   mempool_free(msgBuf, FhgfsOpsRemoting_msgBufPool);

outNoAlloc:

   AtomicInt_max(ioInfo->maxUsedTargetIndex, maxUsedTargetIndex);

out:
   if (unlikely(needReadWriteHandlePages) )
   {
      if (rwType == BEEGFS_RWTYPE_WRITE)
         FhgfsChunkPageVec_iterateAllHandleWritePages(pageVec, -EAGAIN);
      else
         FhgfsChunkPageVec_iterateAllHandleReadErr(pageVec);
   }

   return retVal;
}

/**
 * @param fhgfsInode just a hint currently, may be NULL; used for read from secondary heuristic in
 *        case of buddy mirroring.
 *
 * @return number of bytes read or negative fhgfs error code
 */
ssize_t FhgfsOpsRemoting_readfile(char __user *buf, size_t size, loff_t offset,
   RemotingIOInfo* ioInfo, FhgfsInode* fhgfsInode)
{
   struct iovec iov = {
      .iov_base = (char*) buf,
      .iov_len = size,
   };
   struct iov_iter iter;

   BEEGFS_IOV_ITER_INIT(&iter, READ, &iov, 1, size);

   return FhgfsOpsRemoting_readfileVec(&iter, offset, ioInfo, fhgfsInode);
}

static void readfile_nextIter(CommKitContext* context, ReadfileState* state)
{
   struct ReadfileVecState* vecState = container_of(state, struct ReadfileVecState, base);

   state->data = vecState->data;
}

ssize_t FhgfsOpsRemoting_readfileVec(const struct iov_iter* iter, loff_t offset,
   RemotingIOInfo* ioInfo, FhgfsInode* fhgfsInode)
{
   App* app = ioInfo->app;

   ssize_t retVal = iter->count;
   size_t toBeRead = iter->count;
   struct iov_iter iterCopy = *iter;
   loff_t currentOffset = offset;

   StripePattern* pattern = ioInfo->pattern;
   unsigned chunkSize = StripePattern_getChunkSize(pattern);
   UInt16Vec* targetIDs = pattern->getStripeTargetIDs(pattern);
   unsigned numStripeNodes = UInt16Vec_length(targetIDs);
   const char* fileHandleID = ioInfo->fileHandleID;
   int maxUsedTargetIndex = AtomicInt_read(ioInfo->maxUsedTargetIndex);

   ssize_t usableReadSize = 0; // the amount of usable data that was received from the nodes

   __FhgfsOpsRemoting_logDebugIOCall(__func__, iter->count, offset, ioInfo, NULL);

   while(toBeRead)
   {
      unsigned currentTargetIndex = pattern->getStripeTargetIndex(pattern, currentOffset);
      unsigned numWorks = 0; // number of work items that we added to the queue
      LIST_HEAD(stateList);
      struct ReadfileVecState* state;

      // stripeset-loop: loop over one stripe set (using dynamically determined stripe node
      // indices).
      while(toBeRead && (numWorks < numStripeNodes) )
      {
         size_t currentChunkSize =
            StripePattern_getChunkEnd(pattern, currentOffset) - currentOffset + 1;
         size_t currentReadSize = MIN(currentChunkSize, toBeRead);
         loff_t currentNodeLocalOffset = __FhgfsOpsRemoting_getChunkOffset(
            currentOffset, chunkSize, numStripeNodes, currentTargetIndex);
         struct iov_iter chunkIter;

         state = kmalloc(sizeof(*state), list_empty(&stateList) ? GFP_NOFS : GFP_NOWAIT);
         if (!state)
            break;

         maxUsedTargetIndex = MAX(maxUsedTargetIndex, (int)currentTargetIndex);

         chunkIter = iterCopy;
         iov_iter_truncate(&chunkIter, currentChunkSize);

         // prepare the state information
         FhgfsOpsCommKit_initReadfileState(
            &state->base,
            currentNodeLocalOffset,
            chunkIter.count,
            UInt16Vec_at(targetIDs, currentTargetIndex) );

         state->base.firstWriteDoneForTarget =
            BitStore_getBit(ioInfo->firstWriteDone, currentTargetIndex);

         state->data = chunkIter;

         list_add_tail(&state->base.base.targetInfoList, &stateList);

         // use secondary buddy mirror for odd inode numbers
         if( (StripePattern_getPatternType(pattern) == STRIPEPATTERN_BuddyMirror) )
            state->base.base.useBuddyMirrorSecond =
               fhgfsInode ? (fhgfsInode->vfs_inode.i_ino & 1) : false;

         App_incNumRemoteReads(app);

         // prepare for next loop
         currentOffset += currentReadSize;
         toBeRead -= currentReadSize;
         numWorks++;
         iov_iter_advance(&iterCopy, chunkIter.count);
         currentTargetIndex = (currentTargetIndex + 1) % numStripeNodes;
      }

      if(list_empty(&stateList) )
         return -FhgfsOpsErr_OUTOFMEM;

      // communicate with the nodes
      FhgfsOpsCommKit_readfileV2bCommunicate(app, ioInfo, &stateList, readfile_nextIter, NULL);

      // verify results
      list_for_each_entry(state, &stateList, base.base.targetInfoList)
      {
         if(state->base.base.nodeResult == state->base.expectedNodeResult)
         {
            usableReadSize += state->base.base.nodeResult;
            continue;
         }

         // result not as expected => check cause: end-of-file or error condition

         if(state->base.base.nodeResult >= 0)
         { // we have an end of file here (but some data might have been read)
            retVal = usableReadSize + state->base.base.nodeResult;
         }
         else
         { // error occurred
            FhgfsOpsErr nodeError = -(state->base.base.nodeResult);

            Logger* log = App_getLogger(app);
            const char* logContext = "Remoting (read file)";

            if(nodeError == FhgfsOpsErr_INTERRUPTED) // normal on ctrl+c (=> no logErr() )
               Logger_logFormatted(log, Log_DEBUG, logContext,
                  "Storage targetID: %hu; Msg: %s; FileHandle: %s",
                  state->base.base.targetID, FhgfsOpsErr_toErrString(nodeError), fileHandleID);
            else
               Logger_logErrFormatted(log, logContext,
                  "Error storage targetID: %hu; Msg: %s; FileHandle: %s",
                  state->base.base.targetID, FhgfsOpsErr_toErrString(nodeError), fileHandleID);

            retVal = state->base.base.nodeResult;
         }

         toBeRead = 0; /* abort the read here due to incomplete result/error */
         break;
      } // end of results verification for-loop

      while (!list_empty(&stateList) )
      {
         struct ReadfileVecState* state = list_first_entry(&stateList, struct ReadfileVecState,
            base.base.targetInfoList);

         list_del(&state->base.base.targetInfoList);
         kfree(state);
      }
   } // end of while(toBeRead)

   AtomicInt_max(ioInfo->maxUsedTargetIndex, maxUsedTargetIndex);

   return retVal;
}


FhgfsOpsErr FhgfsOpsRemoting_rename(App* app, const char* oldName, unsigned oldLen,
   DirEntryType entryType, const EntryInfo* fromDirInfo, const char* newName, unsigned newLen,
   const EntryInfo* toDirInfo, const struct FileEvent* event)
{
   FhgfsOpsErr retVal;

   RenameMsg requestMsg;
   RequestResponseNode rrNode = {
      .peer = rrpeer_from_entryinfo(fromDirInfo),
      .nodeStore = app->metaNodes,
      .targetStates = app->metaStateStore,
      .mirrorBuddies = app->metaBuddyGroupMapper
   };
   RequestResponseArgs rrArgs;
   FhgfsOpsErr requestRes;
   RenameRespMsg* renameResp;

   // prepare request

   RenameMsg_initFromEntryInfo(&requestMsg, oldName, oldLen, entryType, fromDirInfo, newName,
      newLen, toDirInfo, event);

   RequestResponseArgs_prepare(&rrArgs, NULL, (NetMessage*)&requestMsg, NETMSGTYPE_RenameResp);

   // communicate
   requestRes = MessagingTk_requestResponseNodeRetryAutoIntr(app, &rrNode, &rrArgs);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   { // clean-up
      retVal = requestRes;
      goto cleanup_request;
   }

   // handle result
   renameResp = (RenameRespMsg*)rrArgs.outRespMsg;
   retVal = (FhgfsOpsErr)RenameRespMsg_getValue(renameResp);

   // clean-up
   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return retVal;
}

FhgfsOpsErr FhgfsOpsRemoting_truncfile(App* app, const EntryInfo* entryInfo, loff_t size,
   const struct FileEvent* event)
{
   Logger* log = App_getLogger(app);
   Config* cfg = App_getConfig(app);
   const char* logContext = "Remoting (trunc file)";

   TruncFileMsg requestMsg;
   RequestResponseNode rrNode = {
      .peer = rrpeer_from_entryinfo(entryInfo),
      .nodeStore = app->metaNodes,
      .targetStates = app->metaStateStore,
      .mirrorBuddies = app->metaBuddyGroupMapper
   };
   RequestResponseArgs rrArgs;
   FhgfsOpsErr requestRes;
   TruncFileRespMsg* truncResp;
   FhgfsOpsErr retVal;

   // prepare request
   TruncFileMsg_initFromEntryInfo(&requestMsg, size, entryInfo, event);

   if(Config_getQuotaEnabled(cfg) )
      NetMessage_addMsgHeaderFeatureFlag((NetMessage*)&requestMsg, TRUNCFILEMSG_FLAG_USE_QUOTA);

   RequestResponseArgs_prepare(&rrArgs, NULL, (NetMessage*)&requestMsg, NETMSGTYPE_TruncFileResp);

   // communicate
   requestRes = MessagingTk_requestResponseNodeRetryAutoIntr(app, &rrNode, &rrArgs);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   { // clean-up
      retVal = requestRes;
      goto cleanup_request;
   }

   // handle result
   truncResp = (TruncFileRespMsg*)rrArgs.outRespMsg;
   retVal = (FhgfsOpsErr)TruncFileRespMsg_getValue(truncResp);

   if(unlikely(retVal != FhgfsOpsErr_SUCCESS && retVal != FhgfsOpsErr_TOOBIG) )
   { // error on server
      int logLevel = (retVal == FhgfsOpsErr_PATHNOTEXISTS) ? Log_DEBUG : Log_NOTICE;

      Logger_logFormatted(log, logLevel, logContext, "TruncFileResp error code: %s",
         FhgfsOpsErr_toErrString(retVal) );
   }

   // clean-up
   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return retVal;
}

FhgfsOpsErr FhgfsOpsRemoting_fsyncfile(RemotingIOInfo* ioInfo, bool forceRemoteFlush,
   bool checkSession, bool doSyncOnClose)
{
   const char* logContext = "Remoting (fsync file)";

   App* app = ioInfo->app;
   Logger* log = App_getLogger(app);

   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;

   StripePattern* pattern = ioInfo->pattern;
   UInt16Vec* targetIDs = pattern->getStripeTargetIDs(pattern);
   ssize_t numStripeTargets = UInt16Vec_length(targetIDs);
   ssize_t numMirrorTargets = (StripePattern_getPatternType(pattern) == STRIPEPATTERN_BuddyMirror)
      ? numStripeTargets
      : 0;

   int i;

   struct FsyncContext context = {
      .userID = FhgfsCommon_getCurrentUserID(),
      .forceRemoteFlush = forceRemoteFlush,
      .checkSession = checkSession,
      .doSyncOnClose = doSyncOnClose,
   };

   INIT_LIST_HEAD(&context.states);

   for(i = 0; i < numStripeTargets + numMirrorTargets; i++)
   {
      struct FsyncState* state = kzalloc(sizeof(*state), GFP_NOFS);
      uint16_t nodeIdx = i < numStripeTargets ? i : i - numStripeTargets;

      if(!state)
      {
         Logger_logErr(log, logContext, "Memory allocation failed.");
         retVal = FhgfsOpsErr_OUTOFMEM;
         goto state_failed;
      }

      FhgfsOpsCommKit_initFsyncState(&context, state, UInt16Vec_at(targetIDs, nodeIdx) );
      state->base.useBuddyMirrorSecond = i >= numStripeTargets;
   }

   FhgfsOpsCommKit_fsyncCommunicate(app, ioInfo, &context);

state_failed:
   while(!list_empty(&context.states) )
   {
      struct FsyncState* state = list_first_entry(&context.states, struct FsyncState,
         base.targetInfoList);

      list_del(&state->base.targetInfoList);
      if(state->base.nodeResult != FhgfsOpsErr_SUCCESS && retVal == FhgfsOpsErr_SUCCESS)
      {
         retVal = -state->base.nodeResult;
         Logger_logFormatted(log, Log_WARNING, logContext,
            "Error storage target: %hu; Msg: %s", state->base.selectedTargetID,
            FhgfsOpsErr_toErrString(retVal) );
      }

      kfree(state);
   }

   return retVal;
}

/**
 * @param ignoreErrors true if success should be returned even if some (or all) known targets
 * returned errors (in which case outSizeTotal/outSizeFree values will only show numbers from
 * targets without errors); local errors (e.g. failed mem alloc) will not be ignored.
 * @return FhgfsOpsErr_UNKNOWNTARGET if no targets are known.
 */
FhgfsOpsErr FhgfsOpsRemoting_statStoragePath(App* app, bool ignoreErrors, int64_t* outSizeTotal,
   int64_t* outSizeFree)
{
   Logger* log = App_getLogger(app);
   const char* logContext = "Remoting (stat storage targets)";

   TargetMapper* targetMapper = App_getTargetMapper(app);

   FhgfsOpsErr retVal = FhgfsOpsErr_OUTOFMEM;

   UInt16List targetIDs;
   UInt16ListIter targetsIter;
   unsigned numTargets;

   struct list_head targetStates;
   struct StatStorageState* state;

   *outSizeTotal = 0;
   *outSizeFree = 0;

   UInt16List_init(&targetIDs);
   INIT_LIST_HEAD(&targetStates);

   TargetMapper_getTargetIDs(targetMapper, &targetIDs);
   numTargets = UInt16List_length(&targetIDs);

   if(unlikely(!numTargets) )
   { // we treat no known storage servers as an error
      UInt16List_uninit(&targetIDs);
      Logger_logFormatted(log, Log_CRITICAL, logContext, "No storage targets known.");
      return FhgfsOpsErr_UNKNOWNTARGET;
   }

   UInt16ListIter_init(&targetsIter, &targetIDs);
   while(!UInt16ListIter_end(&targetsIter) )
   {
      struct StatStorageState* state = kmalloc(sizeof(*state), GFP_NOFS);

      if(!state)
         goto fail_state;

      FhgfsOpsCommKit_initStatStorageState(&targetStates, state,
         UInt16ListIter_value(&targetsIter) );

      UInt16ListIter_next(&targetsIter);
   }

   retVal = FhgfsOpsErr_SUCCESS;
   FhgfsOpsCommKit_statStorageCommunicate(app, &targetStates);

   list_for_each_entry(state, &targetStates, base.targetInfoList)
   {
      if(state->base.nodeResult != FhgfsOpsErr_SUCCESS)
      { // something went wrong with this target
         LogLevel logLevel = ignoreErrors ? Log_DEBUG : Log_WARNING;

         Logger_logFormatted(log, logLevel, logContext,
            "Error target (storage): %hu; Msg: %s",
            state->base.selectedTargetID, FhgfsOpsErr_toErrString(-state->base.nodeResult));

         if(!ignoreErrors)
         {
            retVal = -state->base.nodeResult;
            break;
         }
      }
      else
      { // we got data from this target => add up
         *outSizeTotal += state->totalSize;
         *outSizeFree += state->totalFree;
      }
   }

fail_state:
   while(!list_empty(&targetStates) )
   {
      struct StatStorageState* state = list_first_entry(&targetStates, struct StatStorageState,
         base.targetInfoList);

      list_del(&state->base.targetInfoList);
      kfree(state);
   }

   UInt16List_uninit(&targetIDs);

   return retVal;
}

/**
 * @param size Size of puffer pointed to by @outValue
 * @param outSize size of the extended attribute list. May be larger than buffer, in that case not
 *                the whole list is read.
 */
FhgfsOpsErr FhgfsOpsRemoting_listXAttr(App* app, const EntryInfo* entryInfo, char* outValue,
      size_t size, ssize_t* outSize)
{
   ListXAttrMsg requestMsg;
   FhgfsOpsErr requestRes;

   RequestResponseNode rrNode = {
      .peer = rrpeer_from_entryinfo(entryInfo),
      .nodeStore = app->metaNodes,
      .targetStates = app->metaStateStore,
      .mirrorBuddies = app->metaBuddyGroupMapper
   };
   RequestResponseArgs rrArgs;

   ListXAttrRespMsg* listXAttrResp;
   char* xAttrList;
   int xAttrListSize;
   int listXAttrReturnCode;

   // prepare request msg
   ListXAttrMsg_initFromEntryInfoAndSize(&requestMsg, entryInfo, size);

   RequestResponseArgs_prepare(&rrArgs, NULL, &requestMsg.netMessage, NETMSGTYPE_ListXAttrResp);

   // connect & communicate
   requestRes = MessagingTk_requestResponseNodeRetryAutoIntr(app, &rrNode, &rrArgs);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   {
      // clean up
      goto cleanup_request;
   }

   // handle result
   listXAttrResp = (ListXAttrRespMsg*)rrArgs.outRespMsg;

   listXAttrReturnCode = ListXAttrRespMsg_getReturnCode(listXAttrResp);
   xAttrList = ListXAttrRespMsg_getValueBuf(listXAttrResp);
   xAttrListSize = ListXAttrRespMsg_getSize(listXAttrResp);

   if(listXAttrReturnCode != FhgfsOpsErr_SUCCESS)
   {
      requestRes = listXAttrReturnCode;
   }
   else if(outValue)
   {
      // If outValue != NULL, copy the xattr list to the buffer.
      if(xAttrListSize <= size)
      {
         memcpy(outValue, xAttrList, xAttrListSize);
         *outSize = xAttrListSize;
      }
      else // provided buffer is too small: Error.
      {
         requestRes = FhgfsOpsErr_RANGE;
      }
   }
   else
   {
      // If outValue == NULL, just return the size.
      *outSize = xAttrListSize;
   }

   // clean up
   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return requestRes;
}

extern FhgfsOpsErr FhgfsOpsRemoting_removeXAttr(App* app, const EntryInfo* entryInfo,
      const char* name)
{
   RemoveXAttrMsg requestMsg;
   FhgfsOpsErr requestRes;

   RequestResponseNode rrNode = {
      .peer = rrpeer_from_entryinfo(entryInfo),
      .nodeStore = app->metaNodes,
      .targetStates = app->metaStateStore,
      .mirrorBuddies = app->metaBuddyGroupMapper
   };
   RequestResponseArgs rrArgs;

   RemoveXAttrRespMsg* removeXAttrResp;

   // prepqre request msg
   RemoveXAttrMsg_initFromEntryInfoAndName(&requestMsg, entryInfo, name);

   RequestResponseArgs_prepare(&rrArgs, NULL, &requestMsg.netMessage, NETMSGTYPE_RemoveXAttrResp);

   // connect & communicate
   requestRes = MessagingTk_requestResponseNodeRetryAutoIntr(app, &rrNode, &rrArgs);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   {
      // clean up
      goto cleanup_request;
   }

   // handle result
   removeXAttrResp = (RemoveXAttrRespMsg*)rrArgs.outRespMsg;

   requestRes = RemoveXAttrRespMsg_getValue(removeXAttrResp);

   // clean up
   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return requestRes;
}

/**
 * @param name Null-terminated string containing name of xattr.
 * @param value Buffer containing new contents of xattr.
 * @param size Size of buffer.
 * @param flags Flags files as documented for setxattr syscall (XATTR_CREATE, XATTR_REPLACE).
 */
extern FhgfsOpsErr FhgfsOpsRemoting_setXAttr(App* app, const EntryInfo* entryInfo, const char* name,
      const char* value, const size_t size, int flags)
{
   SetXAttrMsg requestMsg;
   FhgfsOpsErr requestRes;

   RequestResponseNode rrNode = {
      .peer = rrpeer_from_entryinfo(entryInfo),
      .nodeStore = app->metaNodes,
      .targetStates = app->metaStateStore,
      .mirrorBuddies = app->metaBuddyGroupMapper
   };
   RequestResponseArgs rrArgs;

   SetXAttrRespMsg* setXAttrResp;

   if (strlen(name) > __FHGFSOPS_REMOTING_MAX_XATTR_NAME_SIZE) // The attribute name is too long
      return FhgfsOpsErr_RANGE;                                // (if the server side prefix is
                                                               // added).

   if (size > __FHGFSOPS_REMOTING_MAX_XATTR_VALUE_SIZE) // The attribute itself is too large and
      return FhgfsOpsErr_TOOLONG;                       // would not fit into a single NetMsg.

   // prepare request msg
   SetXAttrMsg_initFromEntryInfoNameValueAndSize(&requestMsg, entryInfo, name, value, size, flags);

   RequestResponseArgs_prepare(&rrArgs, NULL, &requestMsg.netMessage, NETMSGTYPE_SetXAttrResp);

   // connect & communicate
   requestRes = MessagingTk_requestResponseNodeRetryAutoIntr(app, &rrNode, &rrArgs);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   {
      // clean up
      goto cleanup_request;
   }

   // handle result
   setXAttrResp = (SetXAttrRespMsg*)rrArgs.outRespMsg;

   requestRes = SetXAttrRespMsg_getValue(setXAttrResp);

   // clean up
   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return requestRes;

}

/**
 * @param name Zero-terminated string containing name of xattr.
 * @param outValue pointer to buffer to store the value in.
 * @param size size of buffer pointed to by @outValue.
 * @param outSize size of the extended attribute value (may be larger than buffer, in that case not
 *                everything is read).
 */
extern FhgfsOpsErr FhgfsOpsRemoting_getXAttr(App* app, const EntryInfo* entryInfo, const char* name,
      void* outValue, size_t size, ssize_t* outSize)
{
   GetXAttrMsg requestMsg;
   FhgfsOpsErr requestRes;

   RequestResponseNode rrNode = {
      .peer = rrpeer_from_entryinfo(entryInfo),
      .nodeStore = app->metaNodes,
      .targetStates = app->metaStateStore,
      .mirrorBuddies = app->metaBuddyGroupMapper
   };
   RequestResponseArgs rrArgs;

   GetXAttrRespMsg* getXAttrResp;
   char* xAttrBuf;
   int xAttrSize;
   int getXAttrReturnCode;

   // prepare request msg
   GetXAttrMsg_initFromEntryInfoNameAndSize(&requestMsg, entryInfo, name, size);

   RequestResponseArgs_prepare(&rrArgs, NULL, &requestMsg.netMessage, NETMSGTYPE_GetXAttrResp);

   // connect & communicate
   requestRes = MessagingTk_requestResponseNodeRetryAutoIntr(app, &rrNode, &rrArgs);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   {
      // clean up
      goto cleanup_request;
   }

   // handle result
   getXAttrResp = (GetXAttrRespMsg*)rrArgs.outRespMsg;

   getXAttrReturnCode = GetXAttrRespMsg_getReturnCode(getXAttrResp);
   xAttrBuf = GetXAttrRespMsg_getValueBuf(getXAttrResp);
   xAttrSize = GetXAttrRespMsg_getSize(getXAttrResp);

   if(getXAttrReturnCode != FhgfsOpsErr_SUCCESS)
   {
      requestRes = getXAttrReturnCode;
   }
   else if(outValue)
   {
      // If outValue != NULL, copy the xattr buffer to the buffer.
      if(xAttrSize <= size)
      {
         memcpy(outValue, xAttrBuf, xAttrSize);
         *outSize = xAttrSize;
      }
      else // provided buffer is too small: Error.
      {
         requestRes = FhgfsOpsErr_RANGE;
      }
   }
   else
   {
      // If outValue == NULL, just return the size.
      *outSize = xAttrSize;
   }

   // clean up
   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return requestRes;
}

/**
 * Lookup or create a file and stat it in a single remote request.
 *
 * @param outInfo outInfo::outEntryInfo attribs set only in case of success (and must then be
 * kfreed by the caller).
 * @return success if we got the basic outInfo::outEntryInfo; this means either file existed
 * and O_EXCL wasn't specified or file didn't exist and was created.
 */
FhgfsOpsErr FhgfsOpsRemoting_lookupIntent(App* app,
   const LookupIntentInfoIn* inInfo, LookupIntentInfoOut* outInfo)
{
   Config* cfg = App_getConfig(app);

   FhgfsOpsErr retVal;

   const CreateInfo* createInfo = inInfo->createInfo;
   const OpenInfo* openInfo = inInfo->openInfo;

   LookupIntentMsg requestMsg;
   RequestResponseNode rrNode = {
      .peer = rrpeer_from_entryinfo(inInfo->parentEntryInfo),
      .nodeStore = app->metaNodes,
      .targetStates = app->metaStateStore,
      .mirrorBuddies = app->metaBuddyGroupMapper
   };
   RequestResponseArgs rrArgs;
   FhgfsOpsErr requestRes;
   LookupIntentRespMsg* lookupResp;

   // prepare request
   if (inInfo->entryInfoPtr)
   {  // EntryInfo already available, so a revalidate intent (lookup-by-id/entryInfo)
      LookupIntentMsg_initFromEntryInfo(&requestMsg, inInfo->parentEntryInfo, inInfo->entryName,
         inInfo->entryInfoPtr);
   }
   else
   {  // no EntryInfo available, we need to lookup-by-name
      LookupIntentMsg_initFromName(&requestMsg, inInfo->parentEntryInfo, inInfo->entryName);
   }

   // always add a stat-intent
   LookupIntentMsg_addIntentStat(&requestMsg);

   if(createInfo)
   {
      LookupIntentMsg_addIntentCreate(&requestMsg, createInfo->userID, createInfo->groupID,
         createInfo->mode, createInfo->umask, createInfo->preferredStorageTargets,
         createInfo->fileEvent);

      if(inInfo->isExclusiveCreate)
         LookupIntentMsg_addIntentCreateExclusive(&requestMsg);
   }

   if(openInfo)
   {
      const NumNodeID localNodeNumID = Node_getNumID(App_getLocalNode(app) );
      LookupIntentMsg_addIntentOpen(&requestMsg, localNodeNumID, openInfo->accessFlags);
   }

   if(Config_getQuotaEnabled(cfg) )
      NetMessage_addMsgHeaderFeatureFlag((NetMessage*)&requestMsg, LOOKUPINTENTMSG_FLAG_USE_QUOTA);

   RequestResponseArgs_prepare(&rrArgs, NULL, (NetMessage*)&requestMsg,
      NETMSGTYPE_LookupIntentResp);

   // communicate
   requestRes = MessagingTk_requestResponseNodeRetryAutoIntr(app, &rrNode, &rrArgs);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   { // communication error
      retVal = requestRes;
      goto cleanup_request;
   }

   // handle result

   lookupResp = (LookupIntentRespMsg*)rrArgs.outRespMsg;

   LookupIntentInfoOut_initFromRespMsg(outInfo, lookupResp);

   // if we got here, the entry lookup or creation was successful
   retVal = FhgfsOpsErr_SUCCESS;

   // clean-up
   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return retVal;
}


#ifdef LOG_DEBUG_MESSAGES
void __FhgfsOpsRemoting_logDebugIOCall(const char* logContext, size_t size, loff_t offset,
   RemotingIOInfo* ioInfo, const char* rwTypeStr)
{
   App* app = ioInfo->app;
   Logger* log = App_getLogger(app);

   if (rwTypeStr)
      Logger_logFormatted(log, Log_DEBUG, logContext,
         "rwType: %s; size: %lld; offset: %lld; fileHandleID: %s openFlags: %d",
         rwTypeStr, (long long)size, (long long)offset, ioInfo->fileHandleID, ioInfo->accessFlags);
   else
      Logger_logFormatted(log, Log_DEBUG, logContext,
         "size: %lld; offset: %lld; fileHandleID: %s",
         (long long)size, (long long)offset, ioInfo->fileHandleID);

}
#endif // LOG_DEBUG_MESSAGES


/**
 * Compute the chunk-file offset on a storage server from a given user file position.
 *
 * Note: Make sure that the given stripeNodeIndex is really correct for the given pos.
 */
static int64_t __FhgfsOpsRemoting_getChunkOffset(int64_t pos, unsigned chunkSize, size_t numNodes,
   size_t stripeNodeIndex)
{
   /* the code below is an optimization (wrt division and modulo) of the following three lines:
         int64_t posModChunkSize = (pos % chunkSize);
         int64_t stripeSetStart = pos - posModChunkSize - (stripeNodeIndex*chunkSize);
         return ( (stripeSetStart / numNodes) + posModChunkSize); */

   /* note: "& (chunksize-1) only works as "% chunksize" replacement, because chunksize must be
      a power of two */

   int64_t posModChunkSize = pos & (chunkSize-1);
   int64_t stripeSetStart = pos - posModChunkSize - (stripeNodeIndex*chunkSize);

   int64_t stripeSetStartDivNumNodes;

   // note: if numNodes is a power of two, we can do bit shifting instead of division

   if(MathTk_isPowerOfTwo(numNodes) )
   { // quick path => bit shifting
      stripeSetStartDivNumNodes = stripeSetStart >> MathTk_log2Int32(numNodes);
   }
   else
   { // slow path => division
      // note: do_div(n64, base32) assigns the result to n64 and returns the remainder!
      // (we need do_div to enable this division on 32bit archs)

      stripeSetStartDivNumNodes = stripeSetStart; // will be changed by do_div()
      do_div(stripeSetStartDivNumNodes, (unsigned)numNodes);
   }

   return (stripeSetStartDivNumNodes + posModChunkSize);
}

FhgfsOpsErr FhgfsOpsRemoting_hardlink(App* app, const char* fromName, unsigned fromLen,
   const EntryInfo* fromInfo, const EntryInfo* fromDirInfo, const char* toName, unsigned toLen,
   const EntryInfo* toDirInfo, const struct FileEvent* event)
{
   Logger* log = App_getLogger(app);

   const char* logContext = "Remoting (hardlink)";

   HardlinkMsg requestMsg;
   RequestResponseNode rrNode = {
      .peer = rrpeer_from_entryinfo(fromDirInfo),
      .nodeStore = app->metaNodes,
      .targetStates = app->metaStateStore,
      .mirrorBuddies = app->metaBuddyGroupMapper
   };
   RequestResponseArgs rrArgs;
   FhgfsOpsErr requestRes;
   HardlinkRespMsg* hardlinkResp;
   FhgfsOpsErr retVal;

   // prepare request
   HardlinkMsg_initFromEntryInfo(&requestMsg, fromDirInfo, fromName, fromLen, fromInfo, toDirInfo,
      toName, toLen, event);

   RequestResponseArgs_prepare(&rrArgs, NULL, (NetMessage*)&requestMsg, NETMSGTYPE_HardlinkResp);

   // communicate
   requestRes = MessagingTk_requestResponseNodeRetryAutoIntr(app, &rrNode, &rrArgs);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   { // clean-up
      retVal = requestRes;
      goto cleanup_request;
   }

   // handle result
   hardlinkResp = (HardlinkRespMsg*)rrArgs.outRespMsg;
   retVal = (FhgfsOpsErr)HardlinkRespMsg_getValue(hardlinkResp);

   if(retVal == FhgfsOpsErr_SUCCESS)
   {
      // success (nothing to be done here)
   }
   else
   {
      int logLevel = Log_NOTICE;

      if( (retVal == FhgfsOpsErr_PATHNOTEXISTS) || (retVal == FhgfsOpsErr_INUSE) ||
          (retVal == FhgfsOpsErr_EXISTS) )
         logLevel = Log_DEBUG; // don't bother user with non-error messages

      Logger_logFormatted(log, logLevel, logContext, "HardlinkResp error code: %s",
         FhgfsOpsErr_toErrString(retVal) );
   }

   // clean-up
   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return retVal;
}


/**
 * Refresh the entry (meta-data update)
 */
FhgfsOpsErr FhgfsOpsRemoting_refreshEntry(App* app, const EntryInfo* entryInfo)
{
   Logger* log = App_getLogger(app);
   const char* logContext = "Remoting (refresh Entry)";

   RefreshEntryInfoMsg requestMsg;
   RequestResponseNode rrNode = {
      .peer = rrpeer_from_entryinfo(entryInfo),
      .nodeStore = app->metaNodes,
      .targetStates = app->metaStateStore,
      .mirrorBuddies = app->metaBuddyGroupMapper
   };
   RequestResponseArgs rrArgs;
   FhgfsOpsErr requestRes;
   RefreshEntryInfoRespMsg* refreshResp;
   FhgfsOpsErr retVal;

   // prepare request
   RefreshEntryInfoMsg_initFromEntryInfo(&requestMsg, entryInfo );

   RequestResponseArgs_prepare(&rrArgs, NULL, (NetMessage*)&requestMsg,
      NETMSGTYPE_RefreshEntryInfoResp);

   // communicate
   requestRes = MessagingTk_requestResponseNodeRetryAutoIntr(app, &rrNode, &rrArgs);

   if(unlikely(requestRes != FhgfsOpsErr_SUCCESS) )
   { // clean-up
      retVal = requestRes;
      goto cleanup_request;
   }

   // handle result
   refreshResp = (RefreshEntryInfoRespMsg*)rrArgs.outRespMsg;
   retVal = (FhgfsOpsErr)RefreshEntryInfoRespMsg_getResult(refreshResp);

   if(retVal != FhgfsOpsErr_SUCCESS)
   {
      LOG_DEBUG_FORMATTED(log, Log_DEBUG, logContext, "StatResp error code: %s",
         FhgfsOpsErr_toErrString(retVal) );
      IGNORE_UNUSED_VARIABLE(log);
      IGNORE_UNUSED_VARIABLE(logContext);
   }


   // clean-up
   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return retVal;
}


FhgfsOpsErr FhgfsOpsRemoting_bumpFileVersion(App* app, const EntryInfo* entryInfo,
   bool persistent, const struct FileEvent* event)
{
   struct BumpFileVersionMsg requestMsg;
   RequestResponseNode rrNode = {
      .peer = rrpeer_from_entryinfo(entryInfo),
      .nodeStore = app->metaNodes,
      .targetStates = app->metaStateStore,
      .mirrorBuddies = app->metaBuddyGroupMapper
   };
   RequestResponseArgs rrArgs;
   FhgfsOpsErr requestRes;
   struct BumpFileVersionRespMsg* bumpResp;
   FhgfsOpsErr retVal;

   // prepare request
   BumpFileVersionMsg_init(&requestMsg, entryInfo, persistent, event);

   RequestResponseArgs_prepare(&rrArgs, NULL, &requestMsg.netMessage,
         NETMSGTYPE_BumpFileVersionResp);

   // communicate
   requestRes = MessagingTk_requestResponseNodeRetryAutoIntr(app, &rrNode, &rrArgs);

   if (requestRes != FhgfsOpsErr_SUCCESS)
   { // clean-up
      retVal = requestRes;
      goto cleanup_request;
   }

   // handle result
   bumpResp = (struct BumpFileVersionRespMsg*) rrArgs.outRespMsg;
   retVal = bumpResp->base.value;

   if (retVal != FhgfsOpsErr_SUCCESS)
   {
      LOG_DEBUG_FORMATTED(app->logger, Log_DEBUG, __func__, "BumpFile error code: %s",
         FhgfsOpsErr_toErrString(retVal));
   }

   // clean-up
   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return retVal;
}

FhgfsOpsErr FhgfsOpsRemoting_getFileVersion(App* app, const EntryInfo* entryInfo,
   uint64_t* outVersion)
{
   struct GetFileVersionMsg requestMsg;
   RequestResponseNode rrNode = {
      .peer = rrpeer_from_entryinfo(entryInfo),
      .nodeStore = app->metaNodes,
      .targetStates = app->metaStateStore,
      .mirrorBuddies = app->metaBuddyGroupMapper
   };
   RequestResponseArgs rrArgs;
   FhgfsOpsErr requestRes;
   struct GetFileVersionRespMsg* getResp;
   FhgfsOpsErr retVal;

   // prepare request
   GetFileVersionMsg_init(&requestMsg, entryInfo);

   RequestResponseArgs_prepare(&rrArgs, NULL, &requestMsg.netMessage,
         NETMSGTYPE_GetFileVersionResp);

   // communicate
   requestRes = MessagingTk_requestResponseNodeRetryAutoIntr(app, &rrNode, &rrArgs);

   if (requestRes != FhgfsOpsErr_SUCCESS)
   { // clean-up
      retVal = requestRes;
      goto cleanup_request;
   }

   // handle result
   getResp = (struct GetFileVersionRespMsg*) rrArgs.outRespMsg;
   retVal = getResp->result;
   *outVersion = getResp->version;

   if (retVal != FhgfsOpsErr_SUCCESS)
   {
      LOG_DEBUG_FORMATTED(app->logger, Log_DEBUG, __func__, "GetFileVersion error code: %s",
         FhgfsOpsErr_toErrString(retVal));
   }

   // clean-up
   RequestResponseArgs_freeRespBuffers(&rrArgs, app);

cleanup_request:
   return retVal;
}
