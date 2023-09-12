#include <common/Common.h> // (placed up here for LINUX_VERSION_CODE definition)


/**
 * NFS export is probably not worth the backport efforts for kernels before 2.6.29
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)

#include <common/storage/Metadata.h>
#include <common/toolkit/StringTk.h>
#include <filesystem/FhgfsInode.h>
#include <filesystem/FhgfsOpsInode.h>
#include <net/filesystem/FhgfsOpsRemoting.h>
#include <os/OsTypeConversion.h>
#include "FhgfsOpsExport.h"
#include "FhgfsOpsHelper.h"
#include "FhgfsOpsFile.h"
#include "FsDirInfo.h"
#include "FhgfsOpsDir.h"



/**
 * Operations for NFS export (and open_by_handle).
 */
const struct export_operations fhgfs_export_ops =
{
   .encode_fh       = FhgfsOpsExport_encodeNfsFileHandle,
   .fh_to_dentry    = FhgfsOpsExport_nfsFileHandleToDentry,
   .fh_to_parent    = FhgfsOpsExport_nfsFileHandleToParent,
   .get_parent      = FhgfsOpsExport_getParentDentry,
   .get_name        = FhgfsOpsExport_getName,
};


// placed here to make it possible to inline it (compiler decision)
static int __FhgfsOpsExport_encodeNfsFileHandleV3(struct inode* inode, __u32* file_handle_buf,
   int* max_len, const EntryInfo* parentInfo);
static bool __FhgfsOpsExport_iterateDirFindName(struct dentry* dentry, const char* entryID,
   char* outName);



/**
 * Single-byte handle type identifier that will be returned by _encodeNfsFileHandle and later be
 * used by _nfsFileHandleToDentry to decode the handle.
 *
 * note: in-tree file systems use "enum fid_type" in linux/exportfs.h; this is our own version of
 * it to let _nfsFileHandleToDentry know how to decode the FhgfsNfsFileHandleV2 structure.
 * note: according to linux/exportfs.h, "the filesystem must not use the value '0' or '0xff'" as
 * valid types; 0xff means given handle buffer size is too small.
 */
enum FhgfsNfsHandleType
{
   FhgfsNfsHandle_STANDARD_V1 = 0xf1,     /* some arbitrary number that doesn't conflict with others
                                             in linux/exportfs.h (though that wouldn't be a real
                                             conflict) to identify our standard valid handle. */
   FhgfsNfsHandle_STANDARD_V2 = 0xf2,     // Adds the parentOwnerNodeID
   FhgfsNfsHandle_STANDARD_V3 = 0xf3,     /* Adds isBuddyMirrored */

   FhgfsNfsHandle_INVALID     = 0xfe,     /* special meaning: error occured, invalid handle
                                             (this is only a hint for _nfsFileHandleToDentry,
                                             because _encodeNfsFileHandle has no way to return an
                                             error to the calling kernel code). */

   FhgfsNfsHandle_BUFTOOSMALL = 0xff,     /* special meaning for callers: given handle buffer
                                             was too small */
};

typedef enum FhgfsNfsHandleType FhgfsNfsHandleType;


/**
 * Encode a file handle (typically for NFS) that can later be used to lookup an inode via
 * _nfsFileHanleToDentry().
 *
 * @param max_len file_handle_buf array length (=> length in 4-byte words), will be set to actually
 * used or desired length.
 * @param parent_inode/connectable if set, this means we should try to create a connectable file
 * handle, so that we can later do a parent dir lookup from it.
 *
 * @return FhgfsNfsHandleType_...
 */
#ifndef KERNEL_HAS_ENCODE_FH_INODE

int FhgfsOpsExport_encodeNfsFileHandle(struct dentry* dentry, __u32* file_handle_buf, int* max_len,
   int connectable)
{
   struct inode* inode = dentry->d_inode;
   struct inode* parent_inode = dentry->d_parent->d_inode;

   if (connectable)
   {
      FhgfsInode* fhgfsParentInode = BEEGFS_INODE(parent_inode);
      const EntryInfo* parentInfo = FhgfsInode_getEntryInfo(fhgfsParentInode);

      return __FhgfsOpsExport_encodeNfsFileHandleV3(inode, file_handle_buf, max_len, parentInfo);
   }
   else
      return __FhgfsOpsExport_encodeNfsFileHandleV3(inode, file_handle_buf, max_len, NULL);
}

#else // LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)

int FhgfsOpsExport_encodeNfsFileHandle(struct inode* inode, __u32* file_handle_buf, int* max_len,
   struct inode* parent_inode)
{
   if (parent_inode)
   {
      FhgfsInode* fhgfsParentInode = BEEGFS_INODE(parent_inode);
      const EntryInfo* parentInfo = FhgfsInode_getEntryInfo(fhgfsParentInode);

      return __FhgfsOpsExport_encodeNfsFileHandleV3(inode, file_handle_buf, max_len, parentInfo);
   }
   else
      return __FhgfsOpsExport_encodeNfsFileHandleV3(inode, file_handle_buf, max_len, NULL);
}

#endif // LINUX_VERSION_CODE

int __FhgfsOpsExport_encodeNfsFileHandleV3(struct inode* inode, __u32* file_handle_buf,
   int* max_len, const EntryInfo* parentInfo)
{
   FhgfsNfsHandleType retVal = FhgfsNfsHandle_STANDARD_V3;
   FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);
   struct FhgfsNfsFileHandleV3* fhgfsNfsHandle = (void*)file_handle_buf;
   __u8* handleAsArray = (__u8 *)fhgfsNfsHandle; // Used for padding

   size_t fhgfsNfsHandleLen = sizeof(struct FhgfsNfsFileHandleV3);
   size_t givenHandleByteLength = (*max_len) * sizeof(__u32);

   const EntryInfo* entryInfo;
   bool parseParentIDRes;
   bool parseEntryIDRes;

   *max_len = (fhgfsNfsHandleLen + sizeof(__u32)-1) / sizeof(__u32); /* set desired/used max_len for
      caller (4-byte words; "+sizeof(u32)-1" rounds up) */

   // check whether given buf length is enough for our handle
   if(givenHandleByteLength < fhgfsNfsHandleLen)
      return FhgfsNfsHandle_BUFTOOSMALL;

   // get entryInfo and serialize it in a special small format
   // (normal string-based serialization would use too much buffer space)


   FhgfsInode_entryInfoReadLock(fhgfsInode);       // LOCK EntryInfo

   entryInfo = FhgfsInode_getEntryInfo(fhgfsInode);

   parseParentIDRes = __FhgfsOpsExport_parseEntryIDForNfsHandle(entryInfo->parentEntryID,
      &fhgfsNfsHandle->parentEntryIDCounter, &fhgfsNfsHandle->parentEntryIDTimestamp,
      &fhgfsNfsHandle->parentEntryIDNodeID);

   if(unlikely(!parseParentIDRes) )
   { // parsing failed (but we have no real way to return an error)
      retVal = FhgfsNfsHandle_INVALID;
      goto cleanup;
   }

   parseEntryIDRes = __FhgfsOpsExport_parseEntryIDForNfsHandle(entryInfo->entryID,
      &fhgfsNfsHandle->entryIDCounter, &fhgfsNfsHandle->entryIDTimestamp,
      &fhgfsNfsHandle->entryIDNodeID);

   if(unlikely(!parseEntryIDRes) )
   { // parsing failed (but we have no real way to return an error)
      retVal = FhgfsNfsHandle_INVALID;
      goto cleanup;
   }

   fhgfsNfsHandle->ownerNodeID = entryInfo->owner.node;
   fhgfsNfsHandle->entryType = entryInfo->entryType;
   fhgfsNfsHandle->isBuddyMirrored = EntryInfo_getIsBuddyMirrored(entryInfo);

   if (parentInfo)
   {
      // NOTE: Does not need to be locked, as it is not a char* value
      fhgfsNfsHandle->parentOwnerNodeID = parentInfo->owner.node;
   }
   else
      fhgfsNfsHandle->parentOwnerNodeID = (NumNodeID){0};

   // Pad remaining space between real file handle length and max_len with zeroes
   for (size_t i = fhgfsNfsHandleLen; i < givenHandleByteLength; i++) {
      handleAsArray[i] = 0x00;
   }

cleanup:
   FhgfsInode_entryInfoReadUnlock(fhgfsInode);       // UNLOCK EntryInfo

   return retVal;
}

/**
 * Lookup an inode based on a file handle that was previously created via _encodeNfsFileHandle().
 *
 * @param fid file handle buffer
 * @param fh_len fid buffer length in 4-byte words
 * @param fileid_type FhgfsNfsHandleType_... (as returned by _encodeNfsFileHandle).
 */
struct dentry* FhgfsOpsExport_nfsFileHandleToDentry(struct super_block *sb, struct fid *fid,
   int fh_len, int fileid_type)
{
   App* app = FhgfsOps_getApp(sb);
   const char* logContext = "NFS-handle-to-dentry";

   FhgfsNfsFileHandleV2 fhgfsNfsHandleV2;
   struct FhgfsNfsFileHandleV3 fhgfsNfsHandleV3;
   struct FhgfsNfsFileHandleV3* fhgfsNfsHandle = (struct FhgfsNfsFileHandleV3*) fid->raw;

   FhgfsNfsHandleType handleType = (FhgfsNfsHandleType)fileid_type;

   void* rawHandle = fid->raw;

   size_t fhgfsNfsHandleLen;
   size_t givenHandleByteLength = fh_len * sizeof(__u32); // fh_len is in 4-byte words

   static bool isFirstCall = true; // true if this method is called for the first time

   FhgfsOpsHelper_logOp(Log_SPAM, app, NULL, NULL, logContext);

   if (unlikely(isFirstCall) )
   {
      /* Our getattr functions assumes it is called after a lookup-intent and therefore does not
       * validate the inode by default. However, this assumption is not true for NFS, and as result
       * inode updates are never detected. NFS even caches readdir results and without inode updates
       * even deleted entries are not detected. So we need to disable the getattr optimization for
       * nfs exports.
       */

      Config* cfg = App_getConfig(app);
      Logger* log = App_getLogger(app);

      Config_setTuneRefreshOnGetAttr(cfg);

      Logger_logFormatted(log, Log_DEBUG, logContext,
         "nfs export detected: auto enabling refresh-on-getattr");

      isFirstCall = false;
   }

   // check if this handle is valid
   if (unlikely(handleType != FhgfsNfsHandle_STANDARD_V1 &&
         handleType != FhgfsNfsHandle_STANDARD_V2 &&
         handleType != FhgfsNfsHandle_STANDARD_V3))
   {
      printk_fhgfs_debug(KERN_INFO, "%s: Called with invalid handle type: 0x%x\n",
         __func__, fileid_type);

      return NULL;
   }

   if (likely(handleType == FhgfsNfsHandle_STANDARD_V3))
      fhgfsNfsHandleLen = sizeof(struct FhgfsNfsFileHandleV3);
   else if (handleType == FhgfsNfsHandle_STANDARD_V2)
      fhgfsNfsHandleLen = sizeof(FhgfsNfsFileHandleV2);
   else
      fhgfsNfsHandleLen = sizeof(FhgfsNfsFileHandleV1);

   // check if given handle length is valid
   if(unlikely(givenHandleByteLength < fhgfsNfsHandleLen) )
   {
      printk_fhgfs_debug(KERN_INFO, "%s: Called with too small handle length: "
         "%d bytes; (handle type: 0x%x)\n",
         __func__, (int)givenHandleByteLength, fileid_type);

      return NULL;
   }

   if (unlikely(handleType == FhgfsNfsHandle_STANDARD_V1))
   {  // old handle, generated before the update that creates V2 handles, convert V1 to V2
      FhgfsNfsFileHandleV1* handleV1 = (FhgfsNfsFileHandleV1*) rawHandle;

      fhgfsNfsHandleV2.entryIDCounter           = handleV1->entryIDCounter;
      fhgfsNfsHandleV2.entryIDNodeID            = handleV1->entryIDNodeID;
      fhgfsNfsHandleV2.entryIDTimestamp         = handleV1->entryIDTimestamp;
      fhgfsNfsHandleV2.ownerNodeID              = handleV1->ownerNodeID;
      fhgfsNfsHandleV2.parentEntryIDCounter     = handleV1->parentEntryIDCounter;
      fhgfsNfsHandleV2.parentEntryIDNodeID      = handleV1->parentEntryIDNodeID;
      fhgfsNfsHandleV2.parentEntryIDTimestamp   = handleV1->parentEntryIDTimestamp;
      fhgfsNfsHandleV2.entryType                = handleV1->entryType;

      fhgfsNfsHandleV2.parentOwnerNodeID = 0; // only available in V2 handles

      rawHandle = &fhgfsNfsHandleV2;
      handleType = FhgfsNfsHandle_STANDARD_V2;
   }

   if (unlikely(handleType == FhgfsNfsHandle_STANDARD_V2))
   {
      FhgfsNfsFileHandleV2* handleV2 = (FhgfsNfsFileHandleV2*) rawHandle;

      fhgfsNfsHandleV3.entryIDCounter            = handleV2->entryIDCounter;
      fhgfsNfsHandleV3.entryIDNodeID.value       = handleV2->entryIDNodeID;
      fhgfsNfsHandleV3.entryIDTimestamp          = handleV2->entryIDTimestamp;
      fhgfsNfsHandleV3.ownerNodeID.value         = handleV2->ownerNodeID;
      fhgfsNfsHandleV3.parentEntryIDCounter      = handleV2->parentEntryIDCounter;
      fhgfsNfsHandleV3.parentEntryIDNodeID.value = handleV2->parentEntryIDNodeID;
      fhgfsNfsHandleV3.parentEntryIDTimestamp    = handleV2->parentEntryIDTimestamp;
      fhgfsNfsHandleV3.entryType                 = handleV2->entryType;
      fhgfsNfsHandleV3.parentOwnerNodeID.value   = handleV2->parentOwnerNodeID;

      fhgfsNfsHandleV3.isBuddyMirrored = 0;

      fhgfsNfsHandle = &fhgfsNfsHandleV3;
      handleType = FhgfsNfsHandle_STANDARD_V3;
   }

   return __FhgfsOpsExport_lookupDentryFromNfsHandle(sb, fhgfsNfsHandle, false);
}

/**
 * Lookup an inode based on a file handle that was previously created via _encodeNfsFileHandle().
 *
 * @param fid file handle buffer
 * @param fh_len fid buffer length in 4-byte words
 * @param fileid_type FhgfsNfsHandleType_... (as returned by _encodeNfsFileHandle).
 *
 * Note: Always a V2 handle
 */
struct dentry* FhgfsOpsExport_nfsFileHandleToParent(struct super_block *sb, struct fid *fid,
   int fh_len, int fileid_type)
{
   App* app = FhgfsOps_getApp(sb);
   const char* logContext = "NFS-handle-to-parent";

   struct FhgfsNfsFileHandleV3* fhgfsNfsHandle = (struct FhgfsNfsFileHandleV3*)fid->raw;

   FhgfsNfsHandleType handleType = (FhgfsNfsHandleType)fileid_type;

   size_t fhgfsNfsHandleLen = sizeof(struct FhgfsNfsFileHandleV3);
   size_t givenHandleByteLength = fh_len * sizeof(__u32); // fh_len is in 4-byte words

   FhgfsOpsHelper_logOp(Log_SPAM, app, NULL, NULL, logContext);

   // check if this handle is valid
   if (unlikely(handleType != FhgfsNfsHandle_STANDARD_V2
         && handleType != FhgfsNfsHandle_STANDARD_V3))
   {  // V1 handles didn't include the parentOwnerNodeID

   #ifdef BEEGFS_DEBUG
      if (handleType != FhgfsNfsHandle_STANDARD_V1)
         printk_fhgfs_debug(KERN_INFO, "%s: Called with invalid handle type: 0x%x\n",
            __func__, fileid_type);
   #endif

      return NULL;
   }

   // check if given handle length is valid
   if(unlikely(givenHandleByteLength < fhgfsNfsHandleLen) )
   {
      printk_fhgfs_debug(KERN_INFO, "%s: Called with too small handle length: "
         "%d bytes; (handle type: 0x%x)\n",
         __func__, (int)givenHandleByteLength, fileid_type);

      return NULL;
   }

   return __FhgfsOpsExport_lookupDentryFromNfsHandle(sb, fhgfsNfsHandle, true);
}


/**
 * Check whether a dentry/inode for the nfs handle exists in the local cache and try server lookup
 * otherwise.
 *
 * @param isParent  if true the parent will be looked up.
 */
struct dentry* __FhgfsOpsExport_lookupDentryFromNfsHandle(struct super_block *sb,
   struct FhgfsNfsFileHandleV3* fhgfsNfsHandle, bool lookupParent)
{
   bool entryRes;
   char* entryID = NULL;
   size_t entryIDLen;
   char* parentEntryID = NULL;
   App* app = FhgfsOps_getApp(sb);
   Logger* log = App_getLogger(app);
   const char* logContext =
      lookupParent ? "NFS-decode-handle-to-parent-dentry" : "NFS-decode-handle-to-dentry";

   ino_t inodeHash; // (simply the inode number for us)
   struct inode* inode;
   FhgfsInodeComparisonInfo comparisonInfo;

   EntryInfo entryInfo;

   struct dentry* resDentry;

   // generate entryID string

   if (!lookupParent)
      entryRes = __FhgfsOpsExport_entryIDFromNfsHandle(fhgfsNfsHandle->entryIDCounter,
         fhgfsNfsHandle->entryIDTimestamp, fhgfsNfsHandle->entryIDNodeID, &entryID);
   else
      entryRes = __FhgfsOpsExport_entryIDFromNfsHandle(fhgfsNfsHandle->parentEntryIDCounter,
         fhgfsNfsHandle->parentEntryIDTimestamp, fhgfsNfsHandle->parentEntryIDNodeID, &entryID);

   if(unlikely(!entryRes) )
      goto err_cleanup_entryids;

   entryIDLen = strlen(entryID);

   if (strncmp(entryID, META_ROOTDIR_ID_STR, entryIDLen) == 0)
      inodeHash = BEEGFS_INODE_ROOT_INO;
   else
      inodeHash = FhgfsInode_generateInodeID(sb, entryID, entryIDLen);

   comparisonInfo.inodeHash = inodeHash;
   comparisonInfo.entryID = entryID;

   inode = ilookup5(sb, inodeHash, __FhgfsOps_compareInodeID,
      &comparisonInfo); // (ilookup5 calls iget() on match)

   if(!inode)
   { // not found in cache => try to get it from mds

      App* app = FhgfsOps_getApp(sb);
      fhgfs_stat fhgfsStat;
      struct kstat kstat;
      FhgfsOpsErr statRes = FhgfsOpsErr_SUCCESS;

      NumNodeID ownerNodeID;

      FhgfsIsizeHints iSizeHints;

      NumNodeID parentNodeID;
      char* statParentEntryID;

      if (!lookupParent)
      {
         // generate parentEntryID string

         bool parentRes = __FhgfsOpsExport_entryIDFromNfsHandle(
            fhgfsNfsHandle->parentEntryIDCounter, fhgfsNfsHandle->parentEntryIDTimestamp,
            fhgfsNfsHandle->parentEntryIDNodeID, &parentEntryID);

         if(unlikely(!parentRes) )
            goto err_cleanup_entryids;

         ownerNodeID = fhgfsNfsHandle->ownerNodeID;
      }
      else
      {  // parentDir
         ownerNodeID = fhgfsNfsHandle->parentOwnerNodeID;

         /* Note: Needs to be special value to tell fhgfs-meta it is unknown!
          * Must not be empty, as this would tell fhgfs-meta it is the root ID. */
         parentEntryID = StringTk_strDup(EntryInfo_PARENT_ID_UNKNOWN);
      }

      FhgfsInode_initIsizeHints(NULL, &iSizeHints);

      // init entry info

      if (fhgfsNfsHandle->isBuddyMirrored)
         EntryInfo_init(&entryInfo, NodeOrGroup_fromGroup(ownerNodeID.value), parentEntryID,
               entryID, StringTk_strDup("<nfs_fh>"), fhgfsNfsHandle->entryType, 0);
      else
         EntryInfo_init(&entryInfo, NodeOrGroup_fromNode(ownerNodeID), parentEntryID, entryID,
               StringTk_strDup("<nfs_fh>"), fhgfsNfsHandle->entryType, 0);

      // communicate

      statRes = FhgfsOpsRemoting_statAndGetParentInfo(app, &entryInfo, &fhgfsStat,
         &parentNodeID, &statParentEntryID);

      // the lookup of parent information may have failed. this can happen if the entry info we
      // tried to stat describes a directory inode that is mirrored, but whose parent directory is
      // not mirrored. similarly, an unmirrored directory with a mirrored parent directory can fail
      // here. try again with the other choice of mirroring flag.
      if (lookupParent && statRes == FhgfsOpsErr_PATHNOTEXISTS)
      {
         entryInfo.featureFlags ^= ENTRYINFO_FEATURE_BUDDYMIRRORED;
         statRes = FhgfsOpsRemoting_statAndGetParentInfo(app, &entryInfo, &fhgfsStat,
            &parentNodeID, &statParentEntryID);
      }

      if(statRes != FhgfsOpsErr_SUCCESS)
         goto err_cleanup_entryinfo;

      // entry found => create inode

      OsTypeConv_kstatFhgfsToOs(&fhgfsStat, &kstat);

      kstat.ino = inodeHash;

      inode = __FhgfsOps_newInode(sb, &kstat, 0, &entryInfo, &iSizeHints);

      if (likely(inode) )
      {
         if (parentNodeID.value)
         {
            FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);

            FhgfsInode_setParentNodeID(fhgfsInode, parentNodeID);
         }

         if (statParentEntryID)
         {  // update the parentEntryID
            FhgfsInode* fhgfsInode = BEEGFS_INODE(inode);

            // make absolute sure we use the right entryInfo
            FhgfsInode_entryInfoWriteLock(fhgfsInode); // L O C K
            EntryInfo_updateSetParentEntryID(&fhgfsInode->entryInfo, statParentEntryID);
            FhgfsInode_entryInfoWriteUnlock(fhgfsInode); // U N L O C K
         }
      }

   }
   else
      SAFE_KFREE(entryID); // ilookup5 found an existing inode, free the comparison entryID

   if (unlikely(!inode) )
      goto err_cleanup_entryinfo;

   // (d_obtain_alias can also handle pointer error codes and NULL)
   resDentry = d_obtain_alias(inode);

   if (resDentry && !IS_ERR(resDentry) )
   {
      #ifndef KERNEL_HAS_S_D_OP
         resDentry->d_op = &fhgfs_dentry_ops;
      #endif // KERNEL_HAS_S_D_OP

      if (unlikely(Logger_getLogLevel(log) >= Log_SPAM) )
      {
         FhgfsInode* fhgfsInode = BEEGFS_INODE(resDentry->d_inode);
         const EntryInfo* entryInfo;

         FhgfsInode_entryInfoReadLock(fhgfsInode); // L O C K fhgfsInode

         entryInfo = FhgfsInode_getEntryInfo(fhgfsInode);

         Logger_logFormatted(log, Log_SPAM, logContext, "result dentry inode id: %s",
            entryInfo->entryID);

         FhgfsInode_entryInfoReadUnlock(fhgfsInode); // U N L O C K fhgfsInode
      }
   }


   return resDentry;

err_cleanup_entryids:
   SAFE_KFREE(parentEntryID);
   SAFE_KFREE(entryID);

   return ERR_PTR(-ESTALE);

err_cleanup_entryinfo:
   EntryInfo_uninit(&entryInfo);

   return ERR_PTR(-ESTALE);
}

/**
 * Parse an entryID string to get its three components.
 *
 * Note: META_ROOTDIR_ID_STR is a special case (all three components are set to 0).
 *
 * @return false on error
 */
bool __FhgfsOpsExport_parseEntryIDForNfsHandle(const char* entryID, uint32_t* outCounter,
   uint32_t* outTimestamp, NumNodeID* outNodeID)
{
   const int numEntryIDComponents = 3; // sscanf must find 3 components in a valid entryID string

   uint32_t nodeID32; //just a tmp variable, because 16-bit outNodeID cannot be used with sscanf %X
   int scanRes;

   if(!strcmp(META_ROOTDIR_ID_STR, entryID) )
   { // special case: this is the root ID, which doesn't have the usual three components
      *outCounter = 0;
      *outTimestamp = 0;
      *outNodeID = (NumNodeID){0};

      return true;
   }

   scanRes = sscanf(entryID, "%X-%X-%X", outCounter, outTimestamp, &nodeID32);

   if(unlikely(scanRes != numEntryIDComponents) )
   { // parsing failed
      printk_fhgfs_debug(KERN_INFO, "%s: Parsing of entryID failed. entryID: %s\n",
         __func__, entryID);

      return false;
   }

   *outNodeID = (NumNodeID){nodeID32};

   return true;
}

/**
 * Generate an entryID string from the NFS handle components.
 *
 * @param outEntryID will be kmalloced on success and needs to be kfree'd by the caller
 * @return false on error
 */
bool __FhgfsOpsExport_entryIDFromNfsHandle(uint32_t counter, uint32_t timestamp,
   NumNodeID nodeID, char** outEntryID)
{
   if(!counter && !timestamp && !nodeID.value)
   { // special case: root ID
      *outEntryID = StringTk_strDup(META_ROOTDIR_ID_STR);
   }
   else
      *outEntryID = StringTk_kasprintf("%X-%X-%X", counter, timestamp, (uint32_t)nodeID.value);

   return (*outEntryID != NULL); // (just in case kmalloc failed)
}


/**
 * getParentDentry - export_operations->get_parent function
 *
 * Get the directory dentry (and inode) that has childDentry
 *
 * Note: We do not lock childDentry's EntryInfo here, as childDentry does not have a connected
 *       path yet, so childDentry's EntryInfo also cannot change.
 */
struct dentry* FhgfsOpsExport_getParentDentry(struct dentry* childDentry)
{
      int retVal = -ESTALE;

      struct super_block* superBlock = childDentry->d_sb;
      App* app = FhgfsOps_getApp(superBlock);
      Logger* log = App_getLogger(app);
      const char* logContext = "Export_getParentDentry";

      struct inode* parentInode;
      struct inode* childInode = childDentry->d_inode;
      FhgfsInode* fhgfsChildInode = BEEGFS_INODE(childInode);

      EntryInfo childEntryInfoCopy;
      FhgfsInodeComparisonInfo comparisonInfo;

      size_t parentIDLen;
      const char* parentEntryID;

      struct dentry* parentDentry = NULL;

      FhgfsInode_entryInfoReadLock(fhgfsChildInode); // L O C K childInode
      EntryInfo_dup(FhgfsInode_getEntryInfo(fhgfsChildInode), &childEntryInfoCopy);
      FhgfsInode_entryInfoReadUnlock(fhgfsChildInode); // U N L O C K childInode

      parentEntryID = EntryInfo_getParentEntryID(&childEntryInfoCopy);


      /* NOTE: IS_ROOT() would not work, as any childDentry is not connected to its parent
       *       and IS_ROOT() would *always* be true here. */
      if (strlen(parentEntryID) == 0)
      {
         /* This points to a bug, as we should never be called for the root dentry and so this means
          * either root was not correctly identified or setting the parentEntryID failed */
         Logger_logErrFormatted(log, Log_ERR, logContext,
            "Bug: Root does not have parentEntryID set!");

         retVal = -EINVAL;
        goto outErr;
      }

      parentIDLen = strlen(parentEntryID);
      comparisonInfo.entryID = parentEntryID;

      if (strncmp(parentEntryID, META_ROOTDIR_ID_STR, parentIDLen) == 0)
         comparisonInfo.inodeHash = BEEGFS_INODE_ROOT_INO; // root inode
      else
         comparisonInfo.inodeHash = FhgfsInode_generateInodeID(superBlock,
            parentEntryID, strlen(parentEntryID) );

      Logger_logFormatted(log, Log_SPAM, logContext, "Find inode for ID: %s inodeHash: %lu",
         comparisonInfo.entryID, comparisonInfo.inodeHash);

      parentInode = ilookup5(superBlock, comparisonInfo.inodeHash, __FhgfsOps_compareInodeID,
         &comparisonInfo); // (ilookup5 calls iget() on match)

      if(!parentInode)
      { // not found in cache => try to get it from mds

         fhgfs_stat fhgfsStat;
         struct kstat kstat;
         FhgfsOpsErr statRes = FhgfsOpsErr_SUCCESS;
         const char* fileName = StringTk_strDup("<nfs_fh>");
         EntryInfo parentInfo;

         NumNodeID parentNodeID = (NumNodeID){0};
         char* parentEntryID = NULL;

         NumNodeID parentOwnerNodeID;

         FhgfsIsizeHints iSizeHints;

         /* Note: Needs to be (any) special value to tell fhgfs-meta it is unknown!
          *       Must not be empty, as this would tell fhgfs-meta it is the root ID. */
         char* grandParentID = StringTk_strDup(EntryInfo_PARENT_ID_UNKNOWN);

         parentOwnerNodeID = FhgfsInode_getParentNodeID(fhgfsChildInode);
         if (parentOwnerNodeID.value == 0)
         {   /* Hmm, so we don't know the real ownerNodeId, we try the childs ID, but if that fails
              * with FhgfsOpsErr_NOTOWNER, we would need to cycle through all meta-targets, which
              * is too slow and we therefore don't do that, but hope the client can
              * recover -ESTALE itself */
            parentOwnerNodeID = childEntryInfoCopy.owner.node;
         }

         // generate parentEntryID string
         if (EntryInfo_getIsBuddyMirrored(&childEntryInfoCopy))
            EntryInfo_init(&parentInfo, NodeOrGroup_fromGroup(parentOwnerNodeID.value),
                  grandParentID, StringTk_strDup(comparisonInfo.entryID), fileName,
                  DirEntryType_DIRECTORY, 0);
         else
            EntryInfo_init(&parentInfo, NodeOrGroup_fromNode(parentOwnerNodeID), grandParentID,
                  StringTk_strDup(comparisonInfo.entryID), fileName, DirEntryType_DIRECTORY, 0);

         // communicate

         FhgfsInode_initIsizeHints(NULL, &iSizeHints);

         statRes = FhgfsOpsRemoting_statAndGetParentInfo(app, &parentInfo, &fhgfsStat,
            &parentNodeID, &parentEntryID);

         // the lookup of parent information may have failed. this can happen if the entry info we
         // tried to stat describes a directory inode that is mirrored, but whose parent directory
         // is not mirrored. similarly, an unmirrored directory with a mirrored parent directory
         // can fail here. try again with the other choice of mirroring flag.
         if (statRes == FhgfsOpsErr_PATHNOTEXISTS)
         {
            parentInfo.featureFlags ^= ENTRYINFO_FEATURE_BUDDYMIRRORED;
            statRes = FhgfsOpsRemoting_statAndGetParentInfo(app, &parentInfo, &fhgfsStat,
               &parentNodeID, &parentEntryID);
         }

         if(statRes != FhgfsOpsErr_SUCCESS)
         {
            EntryInfo_uninit(&parentInfo);
            goto outErr;
         }

         // entry found => create inode

         OsTypeConv_kstatFhgfsToOs(&fhgfsStat, &kstat);

         kstat.ino = comparisonInfo.inodeHash;

         parentInode = __FhgfsOps_newInodeWithParentID(superBlock, &kstat, 0, &parentInfo,
            parentNodeID, &iSizeHints);

         if (likely(parentInode) && parentEntryID)
         {  // update the parentEntryID
            FhgfsInode* parentFhgfsInode = BEEGFS_INODE(parentInode);

            // make absolute sure we use the right entryInfo
            FhgfsInode_entryInfoWriteLock(parentFhgfsInode); // L O C K
            EntryInfo_updateSetParentEntryID(&parentFhgfsInode->entryInfo, parentEntryID);
            FhgfsInode_entryInfoWriteUnlock(parentFhgfsInode); // U N L O C K
         }

      }

      // (d_obtain_alias can also handle pointer error codes and NULL)
      parentDentry = d_obtain_alias(parentInode);

      if (parentDentry && !IS_ERR(parentDentry) )
      {
         #ifndef KERNEL_HAS_S_D_OP
            parentDentry->d_op = &fhgfs_dentry_ops;
         #endif // KERNEL_HAS_S_D_OP
      }

      // printk(KERN_INFO "%s: d_obtain_alias(inode) res: %ld\n", __func__, IS_ERR(parentDentry) );

      EntryInfo_uninit(&childEntryInfoCopy);

      return parentDentry;

outErr:
   EntryInfo_uninit(&childEntryInfoCopy);
   return ERR_PTR(retVal);
}



/**
 * getName - export_operations->get_name function
 *
 * calls readdir on the parent until it finds an entry with
 * the same entryID as the child, and returns that.

 * @param dirDentry the directory in which to find a name
 * @param outName   a pointer to a %NAME_MAX+1 char buffer to store the name
 * @param child     the dentry for the child directory.
 */
int FhgfsOpsExport_getName(struct dentry* dirDentry, char *outName, struct dentry *child)
{
   struct inode *dirInode = dirDentry->d_inode;

   struct inode *childInode = child->d_inode;
   FhgfsInode* fhgfsChildInode = BEEGFS_INODE(childInode);
   EntryInfo childEntryInfoCopy;
   const char* childEntryID;


   int retVal;
   bool findRes;

   retVal = -ENOTDIR;
   if (!dirInode || !S_ISDIR(dirInode->i_mode))
      goto out;

   retVal = -EINVAL;
   if (!dirInode->i_fop)
      goto out;

   FhgfsInode_entryInfoReadLock(fhgfsChildInode); // LOCK childInode
   EntryInfo_dup(FhgfsInode_getEntryInfo(fhgfsChildInode), &childEntryInfoCopy);
   FhgfsInode_entryInfoReadUnlock(fhgfsChildInode); // UNLOCK childInode

   childEntryID = EntryInfo_getEntryID(&childEntryInfoCopy);

   findRes = __FhgfsOpsExport_iterateDirFindName(dirDentry, childEntryID, outName);
   if (!findRes)
   {
      retVal = -ESTALE;
      goto outUnitEntryInfo;
   }

   retVal = 0;


outUnitEntryInfo:
   EntryInfo_uninit(&childEntryInfoCopy);

out:
   return retVal;
}


/**
 * Find the name of an entry with the given entryID in the directory dirDentry.
 *
 * Note: This uses a rather slow client-side readdir() to find the entry.
 *       Maybe we should add another NetMsg and do it directly on the server!
 */
bool __FhgfsOpsExport_iterateDirFindName(struct dentry* dirDentry, const char* entryID,
   char* outName)
{
   struct super_block* superBlock = dirDentry->d_sb;
   App* app = FhgfsOps_getApp(superBlock);
   Logger* log = App_getLogger(app);
   const char* logContext = "FhgfsOpsExport_readdirFindName";

   bool retVal = false;
   struct inode* dirInode = dirDentry->d_inode;
   FhgfsInode* fhgfsDirInode = BEEGFS_INODE(dirInode);

   size_t contentsPos = 0;
   size_t contentsLength;

   StrCpyVec* dirContents;
   StrCpyVec* dirContentIDs;

   EntryInfo dirEntryInfoCopy;
   FsDirInfo dirInfo;

   FsDirInfo_init(&dirInfo, app);

   dirContents = FsDirInfo_getDirContents(&dirInfo);
   dirContentIDs = FsDirInfo_getEntryIDs(&dirInfo);

   FhgfsInode_entryInfoReadLock(fhgfsDirInode); // L O C K EntryInfo
   EntryInfo_dup(FhgfsInode_getEntryInfo(fhgfsDirInode), &dirEntryInfoCopy);
   FhgfsInode_entryInfoReadUnlock(fhgfsDirInode); // U N L O C K EntryInfo

   if(unlikely(Logger_getLogLevel(log) >= 5) )
   {
      const EntryInfo* dirInfo = FhgfsInode_getEntryInfo(fhgfsDirInode);

      struct inode* inode = dirDentry->d_inode;

      FhgfsOpsHelper_logOpMsg(Log_SPAM, app, dirDentry, inode, logContext,
         "dir-id: %s searchID: %s", dirInfo->entryID, entryID);
   }


   for( ; ; ) // loop as long as we didn't find entryID and as long as the dir has entries
   {
      int refreshRes;
      char* currentName;
      const char* currentEntryID;

      refreshRes = FhgfsOpsHelper_refreshDirInfoIncremental(app,
         &dirEntryInfoCopy, &dirInfo, false);
      if(unlikely(refreshRes) )
      { // error occurred
         break;
      }

      contentsPos = FsDirInfo_getCurrentContentsPos(&dirInfo);
      contentsLength = StrCpyVec_length(dirContents);

#if 0
      LOG_DEBUG_FORMATTED(log, Log_SPAM, logContext,
         "contentsPos: %lld/%lld, endOfDir: %s",
         (long long)contentsPos,  (long long)contentsLength,
         FsDirInfo_getEndOfDir(&dirInfo) ? "yes" : "no");
#endif

      // refreshDirInfoInc guarantees that we either have a valid range for current offset
      // or that contentsLength is empty
      if(!contentsLength)
      { // end of dir
         LOG_DEBUG(log, Log_SPAM, logContext, "reached end of dir");
         break;
      }

      currentName = StrCpyVec_at(dirContents, contentsPos);
      currentEntryID = StrCpyVec_at(dirContentIDs, contentsPos);

      LOG_DEBUG_FORMATTED(log, Log_SPAM, logContext,
         "searchID: %s current dir-entry: %s entryID: %s ",
         entryID, currentName, currentEntryID);

      if(!strcmp(currentEntryID, entryID) )
      { // match found
         // note: out name buf is guaranteed to be NAME_MAX+1 according to <linux/exportfs.h>
         StringTk_strncpyTerminated(outName, currentName, NAME_MAX+1);
         LOG_DEBUG_FORMATTED(log, Log_SPAM, logContext, "Found childName: %s", outName);

         retVal = true;
         break;
      }

      FsDirInfo_setCurrentContentsPos(&dirInfo, contentsPos + 1);
   } // end of for-loop

   // clean-up
   FsDirInfo_uninit((FsObjectInfo*) &dirInfo);
   EntryInfo_uninit(&dirEntryInfoCopy);

   return retVal;
}


#endif // LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
