#ifndef STORAGEDEFINITIONS_H_
#define STORAGEDEFINITIONS_H_

/*
 * Remember to keep these definitions in sync with StorageDefinitions.h of fhgfs_common!!!
 */

#include <common/Common.h>


// open rw flags
#define OPENFILE_ACCESS_READ           1
#define OPENFILE_ACCESS_WRITE          2
#define OPENFILE_ACCESS_READWRITE      4
// open extra flags
#define OPENFILE_ACCESS_APPEND         8
#define OPENFILE_ACCESS_TRUNC         16
#define OPENFILE_ACCESS_DIRECT        32 /* for direct IO */
#define OPENFILE_ACCESS_SYNC          64 /* for sync'ed IO */
#define OPENFILE_ACCESS_NONBLOCKING   128 /* for non-blocking IO (O_NONBLOCK) */
// open masks
#define OPENFILE_ACCESS_MASK_RW       \
   (OPENFILE_ACCESS_READ | OPENFILE_ACCESS_WRITE | OPENFILE_ACCESS_READWRITE)
#define OPENFILE_ACCESS_MASK_EXTRA    (~OPENFILE_ACCESS_MASK_RW)


// set attribs flags
#define SETATTR_CHANGE_MODE               1
#define SETATTR_CHANGE_USERID             2
#define SETATTR_CHANGE_GROUPID            4
#define SETATTR_CHANGE_MODIFICATIONTIME   8
#define SETATTR_CHANGE_LASTACCESSTIME    16


// entry and append lock type flags
#define ENTRYLOCKTYPE_UNLOCK              1
#define ENTRYLOCKTYPE_EXCLUSIVE           2
#define ENTRYLOCKTYPE_SHARED              4
#define ENTRYLOCKTYPE_NOWAIT              8 /* operation may not block if lock not available */
#define ENTRYLOCKTYPE_CANCEL             16 /* cancel all granted and pending locks for given handle
                                               (normally on client file close) */
// entry lock mask
#define ENTRYLOCKTYPE_LOCKOPS_ADD       (ENTRYLOCKTYPE_EXCLUSIVE | ENTRYLOCKTYPE_SHARED)
#define ENTRYLOCKTYPE_LOCKOPS_REMOVE    (ENTRYLOCKTYPE_UNLOCK | ENTRYLOCKTYPE_CANCEL)


struct SettableFileAttribs;
typedef struct SettableFileAttribs SettableFileAttribs;


enum DirEntryType
{ // don't use these directly, use the DirEntryType_IS...() macros below to check entry types
   DirEntryType_INVALID = 0, DirEntryType_DIRECTORY = 1, DirEntryType_REGULARFILE = 2,
   DirEntryType_SYMLINK = 3, DirEntryType_BLOCKDEV = 4, DirEntryType_CHARDEV = 5,
   DirEntryType_FIFO = 6, DirEntryType_SOCKET = 7
};
typedef enum DirEntryType DirEntryType;

#define DirEntryType_ISVALID(dirEntryType)       (dirEntryType != DirEntryType_INVALID)
#define DirEntryType_ISDIR(dirEntryType)         (dirEntryType == DirEntryType_DIRECTORY)
#define DirEntryType_ISREGULARFILE(dirEntryType) (dirEntryType == DirEntryType_REGULARFILE)
#define DirEntryType_ISSYMLINK(dirEntryType)     (dirEntryType == DirEntryType_SYMLINK)
#define DirEntryType_ISBLOCKDEV(dirEntryType)    (dirEntryType == DirEntryType_BLOCKDEV)
#define DirEntryType_ISCHARDEV(dirEntryType)     (dirEntryType == DirEntryType_CHARDEV)
#define DirEntryType_ISFIFO(dirEntryType)        (dirEntryType == DirEntryType_FIFO)
#define DirEntryType_ISSOCKET(dirEntryType)      (dirEntryType == DirEntryType_SOCKET)

/* @return true for any kind of file, including symlinks and special files */
#define DirEntryType_ISFILE(dirEntryType)        ( (dirEntryType >= 2) && (dirEntryType <= 7) )


// hint (but not strict enforcement) that the inode is inlined into the dentry
#define STATFLAG_HINT_INLINE           1




enum EntryLockRequestType
   {EntryLockRequestType_ENTRYFLOCK=0, EntryLockRequestType_RANGEFLOCK=1,
      EntryLockRequestType_ENTRYCOHERENCE=2, EntryLockRequestType_RANGECOHERENCE=3};
typedef enum EntryLockRequestType EntryLockRequestType;



/**
 * Note: Typically used in combination with a value of SETATTR_CHANGE_...-Flags to determine which
 * of the fields are actually used
 */
struct SettableFileAttribs
{
   int mode;
   unsigned userID;
   unsigned groupID;
   int64_t modificationTimeSecs; // unix mtime
   int64_t lastAccessTimeSecs;   // unix atime
};

enum DataStates {
    DATASTATE_AVAILABLE       = 0, // 000 (0) - Default: present on BeeGFS
    DATASTATE_MANUAL_RESTORE  = 1, // 001 (1)
    DATASTATE_AUTO_RESTORE    = 2, // 010 (2)
    DATASTATE_DELAYED_RESTORE = 3, // 011 (3)
    DATASTATE_UNAVAILABLE     = 4, // 100 (4)
    DATASTATE_RESERVED5       = 5, // 101 (5)
    DATASTATE_RESERVED6       = 6, // 110 (6)
    DATASTATE_RESERVED7       = 7, // 111 (7)
};
typedef enum DataStates DataState;

// These must match the definitions in FileInodeStorageData.h on the server side.
#define ACCESS_FLAGS_MASK    0x1F  // 0001 1111 (5 bits)
#define DATA_STATE_MASK      0xE0  // 1110 0000 (3 bits)
#define DATA_STATE_SHIFT     5     // Number of bits to shift

#endif /*STORAGEDEFINITIONS_H_*/
