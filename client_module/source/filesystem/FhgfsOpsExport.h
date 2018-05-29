#ifndef FHGFSOPSEXPORT_H_
#define FHGFSOPSEXPORT_H_

#include <common/Common.h> // (placed up here for LINUX_VERSION_CODE definition)


/**
 * NFS export is probably not worth the backport efforts for kernels before 2.6.29
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)


#include <linux/exportfs.h>


struct FhgfsNfsFileHandleV1;
typedef struct FhgfsNfsFileHandleV1 FhgfsNfsFileHandleV1;

struct FhgfsNfsFileHandleV2;
typedef struct FhgfsNfsFileHandleV2 FhgfsNfsFileHandleV2;

struct FhgfsNfsFileHandleV3;

extern const struct export_operations fhgfs_export_ops;


#ifndef KERNEL_HAS_ENCODE_FH_INODE
int FhgfsOpsExport_encodeNfsFileHandle(struct dentry* dentry, __u32* file_handle_buf, int* max_len,
   int connectable);
#else
int FhgfsOpsExport_encodeNfsFileHandle(struct inode* inode, __u32* file_handle_buf, int* max_len,
   struct inode* parent_inode);
#endif

struct dentry* FhgfsOpsExport_nfsFileHandleToDentry(struct super_block *sb, struct fid *fid,
   int fh_len, int fileid_type);
struct dentry* FhgfsOpsExport_nfsFileHandleToParent(struct super_block *sb, struct fid *fid,
   int fh_len, int fileid_type);

int FhgfsOpsExport_getName(struct dentry* dirDentry, char *outName, struct dentry *child);



struct dentry* __FhgfsOpsExport_lookupDentryFromNfsHandle(struct super_block *sb,
   struct FhgfsNfsFileHandleV3* fhgfsNfsHandle, bool isParent);
struct dentry* FhgfsOpsExport_getParentDentry(struct dentry* childDentry);

bool __FhgfsOpsExport_parseEntryIDForNfsHandle(const char* entryID, uint32_t* outCounter,
   uint32_t* outTimestamp, NumNodeID* outNodeID);
bool __FhgfsOpsExport_entryIDFromNfsHandle(uint32_t counter, uint32_t timestamp,
   NumNodeID nodeID, char** outEntryID);



/**
 * The structure of the handle that will be encoded by _encodeFileHandle().
 *
 * EntryID string format (except for "root"): <u32hex_counter>-<u32hex_timestamp>-<u16hex_nodeID>
 */
struct FhgfsNfsFileHandleV1
{
   // note: fields not in natural order for better alignment and packed size

   uint32_t parentEntryIDCounter;    // from EntryInfo::parentEntryID
   uint32_t parentEntryIDTimestamp;  // from EntryInfo::parentEntryID
   uint32_t entryIDCounter;          // from EntryInfo::entryID
   uint32_t entryIDTimestamp;        // from EntryInfo::entryID
   uint16_t parentEntryIDNodeID;     // from EntryInfo::parentEntryID
   uint16_t entryIDNodeID;           // from EntryInfo::entryID
   uint16_t ownerNodeID;             // from EntryInfo::ownerNodeID
   char     entryType;               // from EntryInfo::entryType

} __attribute__((packed));

/**
 * The structure of the handle that will be encoded by _encodeFileHandle().
 *
 * EntryID string format (except for "root"): <u32hex_counter>-<u32hex_timestamp>-<u16hex_nodeID>
 *
 * Note: V2 adds parentOwnerNodeID
 */
struct FhgfsNfsFileHandleV2
{
   // note: fields not in natural order for better alignment and packed size

   uint32_t parentEntryIDCounter;    // from EntryInfo::parentEntryID
   uint32_t parentEntryIDTimestamp;  // from EntryInfo::parentEntryID
   uint32_t entryIDCounter;          // from EntryInfo::entryID
   uint32_t entryIDTimestamp;        // from EntryInfo::entryID
   uint16_t parentEntryIDNodeID;     // from EntryInfo::parentEntryID
   uint16_t entryIDNodeID;           // from EntryInfo::entryID
   uint16_t parentOwnerNodeID;       // from EntryInfo::ownerNodeID -> parentEntryInfo!
   uint16_t ownerNodeID;             // from EntryInfo::ownerNodeID
   char     entryType;               // from EntryInfo::entryType

} __attribute__((packed));

struct FhgfsNfsFileHandleV3
{
   uint32_t parentEntryIDCounter;    // from EntryInfo::parentEntryID
   uint32_t parentEntryIDTimestamp;  // from EntryInfo::parentEntryID
   uint32_t entryIDCounter;          // from EntryInfo::entryID
   uint32_t entryIDTimestamp;        // from EntryInfo::entryID
   NumNodeID parentEntryIDNodeID;    // from EntryInfo::parentEntryID
   NumNodeID entryIDNodeID;          // from EntryInfo::entryID
   NumNodeID parentOwnerNodeID;      // from EntryInfo::ownerNodeID -> parentEntryInfo!
   NumNodeID ownerNodeID;            // from EntryInfo::ownerNodeID
   char     entryType;               // from EntryInfo::entryType
   uint8_t  isBuddyMirrored;         // from EntryInfo::isBuddyMirrored
} __attribute__((packed));


#endif // LINUX_VERSION_CODE

#endif /* FHGFSOPSEXPORT_H_ */
