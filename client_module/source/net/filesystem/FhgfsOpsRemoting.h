#ifndef FHGFSOPSREMOTING_H_
#define FHGFSOPSREMOTING_H_

#include <common/Common.h>
#include <filesystem/FsObjectInfo.h>
#include <filesystem/FsDirInfo.h>
#include <filesystem/FsFileInfo.h>
#include <common/storage/StorageDefinitions.h>
#include <common/toolkit/MetadataTk.h>
#include <common/storage/FileEvent.h>
#include <common/storage/StorageErrors.h>
#include <net/filesystem/FhgfsOpsCommKit.h>
#include <net/filesystem/RemotingIOInfo.h>
#include <os/iov_iter.h>
#include <toolkit/FhgfsPage.h>
#include <toolkit/FhgfsChunkPageVec.h>

enum Fhgfs_RWType;
typedef enum Fhgfs_RWType Fhgfs_RWType;

struct FileOpVecState {
   FileOpState base;

   struct iov_iter data;
};

union RWFileVecState {
   struct FileOpVecState read;
   struct FileOpState write;
};


struct StripePattern; // forward declaration
struct NetMessage; // forward declaration

extern bool FhgfsOpsRemoting_initMsgBufCache(void);
extern void FhgfsOpsRemoting_destroyMsgBufCache(void);

extern FhgfsOpsErr FhgfsOpsRemoting_listdirFromOffset(const EntryInfo* entryInfo,
   FsDirInfo* dirInfo, unsigned maxOutNames);
extern FhgfsOpsErr FhgfsOpsRemoting_statRoot(App* app, fhgfs_stat* outFhgfsStat);
static inline FhgfsOpsErr FhgfsOpsRemoting_statDirect(App* app, const EntryInfo* entryInfo,
   fhgfs_stat* outFhgfsStat);
extern FhgfsOpsErr FhgfsOpsRemoting_statDirect(App* app, const EntryInfo* entryInfo,
   fhgfs_stat* outFhgfsStat);
extern FhgfsOpsErr FhgfsOpsRemoting_statAndGetParentInfo(App* app, const EntryInfo* entryInfo,
   fhgfs_stat* outFhgfsStat, NumNodeID* outParentNodeID, char** outParentEntryID);
extern FhgfsOpsErr FhgfsOpsRemoting_setAttr(App* app, const EntryInfo* entryInfo,
   SettableFileAttribs* fhgfsAttr, int validAttribs, const struct FileEvent* event);
extern FhgfsOpsErr FhgfsOpsRemoting_mkdir(App* app, const EntryInfo* parentInfo,
   struct CreateInfo* createInfo, EntryInfo* outEntryInfo);
extern FhgfsOpsErr FhgfsOpsRemoting_rmdir(App* app, const EntryInfo* parentInfo,
   const char* entryName, const struct FileEvent* event);
extern FhgfsOpsErr FhgfsOpsRemoting_mkfile(App* app, const EntryInfo* parentInfo,
   struct CreateInfo* createInfo, EntryInfo* outEntryInfo);
extern FhgfsOpsErr FhgfsOpsRemoting_mkfileWithStripeHints(App* app, const EntryInfo* parentInfo,
   struct CreateInfo* createInfo, unsigned numtargets, unsigned chunksize, EntryInfo* outEntryInfo);
extern FhgfsOpsErr FhgfsOpsRemoting_unlinkfile(App* app, const EntryInfo* parentInfo,
   const char* entryName, const struct FileEvent* event);
extern FhgfsOpsErr FhgfsOpsRemoting_openfile(const EntryInfo* entryInfo, RemotingIOInfo* ioInfo,
   uint64_t* outVersion, const struct FileEvent* event);
extern FhgfsOpsErr FhgfsOpsRemoting_closefile(const EntryInfo* entryInfo,
   RemotingIOInfo* ioInfo, const struct FileEvent* event);
extern FhgfsOpsErr FhgfsOpsRemoting_closefileEx(const EntryInfo* entryInfo,
   RemotingIOInfo* ioInfo, bool allowRetries, const struct FileEvent* event);
extern FhgfsOpsErr FhgfsOpsRemoting_flockAppendEx(const EntryInfo* entryInfo, RWLock* eiRLock,
   App* app, const char* fileHandleID, int64_t clientFD, int ownerPID, int lockTypeFlags,
   bool allowRetries);
extern FhgfsOpsErr FhgfsOpsRemoting_flockEntryEx(const EntryInfo* entryInfo, RWLock* eiRLock,
   App* app, const char* fileHandleID, int64_t clientFD, int ownerPID, int lockTypeFlags,
   bool allowRetries);
extern FhgfsOpsErr FhgfsOpsRemoting_flockRangeEx(const EntryInfo* entryInfo, RWLock* eiRLock,
   App* app, const char* fileHandleID, int ownerPID, int lockTypeFlags, uint64_t start, uint64_t end,
   bool allowRetries);

extern ssize_t FhgfsOpsRemoting_writefileVec(struct iov_iter* iter, loff_t offset,
   RemotingIOInfo* ioInfo, bool serializeWrites);

static inline ssize_t FhgfsOpsRemoting_writefile(const char __user *buf, size_t size, loff_t offset,
   RemotingIOInfo* ioInfo)
{
   struct iov_iter *iter = STACK_ALLOC_BEEGFS_ITER_IOV(buf, size, WRITE);
   return FhgfsOpsRemoting_writefileVec(iter, offset, ioInfo, false);
}

static inline ssize_t FhgfsOpsRemoting_writefile_kernel(const char *buf, size_t size, loff_t offset,
   RemotingIOInfo* ioInfo)
{
   struct iov_iter *iter = STACK_ALLOC_BEEGFS_ITER_KVEC(buf, size, WRITE);
   return FhgfsOpsRemoting_writefileVec(iter, offset, ioInfo, false);
}

extern ssize_t FhgfsOpsRemoting_rwChunkPageVec(FhgfsChunkPageVec *pageVec, RemotingIOInfo* ioInfo,
   Fhgfs_RWType rwType);
extern ssize_t FhgfsOpsRemoting_readfileVec(struct iov_iter *iter, size_t size, loff_t offset,
   RemotingIOInfo* ioInfo, FhgfsInode* fhgfsInode);

static inline ssize_t FhgfsOpsRemoting_readfile_user(char __user *buf, size_t size, loff_t offset,
   RemotingIOInfo* ioInfo, FhgfsInode* fhgfsInode)
{
   struct iov_iter *iter = STACK_ALLOC_BEEGFS_ITER_IOV(buf, size, READ);
   return FhgfsOpsRemoting_readfileVec(iter, size, offset, ioInfo, fhgfsInode);
}

static inline ssize_t FhgfsOpsRemoting_readfile_kernel(char *buf, size_t size, loff_t offset,
   RemotingIOInfo* ioInfo, FhgfsInode* fhgfsInode)
{
   struct iov_iter *iter = STACK_ALLOC_BEEGFS_ITER_KVEC(buf, size, READ);
   return FhgfsOpsRemoting_readfileVec(iter, size, offset, ioInfo, fhgfsInode);
}

extern FhgfsOpsErr FhgfsOpsRemoting_rename(App* app, const char* oldName, unsigned oldLen,
   DirEntryType entryType, const EntryInfo* fromDirInfo, const char* newName, unsigned newLen,
   const EntryInfo* toDirInfo, const struct FileEvent* event);
extern FhgfsOpsErr FhgfsOpsRemoting_truncfile(App* app, const EntryInfo* entryInfo, loff_t size,
   const struct FileEvent* event);
extern FhgfsOpsErr FhgfsOpsRemoting_fsyncfile(RemotingIOInfo* ioInfo, bool forceRemoteFlush,
   bool checkSession, bool doSyncOnClose);
extern FhgfsOpsErr FhgfsOpsRemoting_statStoragePath(App* app, bool ignoreErrors,
   int64_t* outSizeTotal, int64_t* outSizeFree);
extern FhgfsOpsErr FhgfsOpsRemoting_listXAttr(App* app, const EntryInfo* entryInfo, char* value,
      size_t size, ssize_t* outSize);
extern FhgfsOpsErr FhgfsOpsRemoting_getXAttr(App* app, const EntryInfo* entryInfo, const char* name,
      void* value, size_t size, ssize_t* outSize);
extern FhgfsOpsErr FhgfsOpsRemoting_removeXAttr(App* app, const EntryInfo* entryInfo,
      const char* name);
extern FhgfsOpsErr FhgfsOpsRemoting_setXAttr(App* app, const EntryInfo* entryInfo, const char* name,
      const char* value, const size_t size, int flags);


extern FhgfsOpsErr FhgfsOpsRemoting_lookupIntent(App* app,
   const LookupIntentInfoIn* inInfo, LookupIntentInfoOut* outInfo);

extern FhgfsOpsErr FhgfsOpsRemoting_hardlink(App* app, const char* fromName, unsigned fromLen,
   const EntryInfo* fromInfo, const EntryInfo* fromDirInfo, const char* toName, unsigned toLen,
   const EntryInfo* toDirInfo, const struct FileEvent* event);

extern FhgfsOpsErr FhgfsOpsRemoting_refreshEntry(App* app, const EntryInfo* entryInfo);

extern FhgfsOpsErr FhgfsOpsRemoting_bumpFileVersion(App* app, const EntryInfo* entryInfo,
   bool persistent, const struct FileEvent* event);
extern FhgfsOpsErr FhgfsOpsRemoting_getFileVersion(App* app, const EntryInfo* entryInfo,
   uint64_t* outVersion);

FhgfsOpsErr __FhgfsOpsRemoting_flockGenericEx(struct NetMessage* requestMsg, unsigned respMsgType,
   NodeOrGroup owner, bool isBuddyMirrored, App* app, const char* fileHandleID, int lockTypeFlags,
   char* lockAckID, bool allowRetries, RWLock* eiRLock);

#ifdef LOG_DEBUG_MESSAGES
extern void __FhgfsOpsRemoting_logDebugIOCall(const char* logContext, size_t size, loff_t offset,
   RemotingIOInfo* ioInfo, const char* rwTypeStr);
#else
#define __FhgfsOpsRemoting_logDebugIOCall(logContext, size, offset, ioInfo, rwTypeStr)
#endif // LOG_DEBUG_MESSAGES


enum Fhgfs_RWType
{
   BEEGFS_RWTYPE_READ = 0,  // read request
   BEEGFS_RWTYPE_WRITE      // write request
};


/**
 * Stat a file or directory using EntryInfo.
 */
FhgfsOpsErr FhgfsOpsRemoting_statDirect(App* app, const EntryInfo* entryInfo,
   fhgfs_stat* outFhgfsStat)
{
   return FhgfsOpsRemoting_statAndGetParentInfo(app, entryInfo, outFhgfsStat, NULL, NULL);
}


#endif /*FHGFSOPSREMOTING_H_*/
