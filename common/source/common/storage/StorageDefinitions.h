#ifndef STORAGEDEFINITIONS_H_
#define STORAGEDEFINITIONS_H_

/*
 * Remember to keep these definitions in sync with StorageDefinitions.h of fhgfs_client_module!!!
 */

#include <common/Common.h>
#include <common/toolkit/serialization/Serialization.h>


// open rw flags
#define OPENFILE_ACCESS_READ           1
#define OPENFILE_ACCESS_WRITE          2
#define OPENFILE_ACCESS_READWRITE      4
// open extra flags
#define OPENFILE_ACCESS_APPEND         8
#define OPENFILE_ACCESS_TRUNC         16
#define OPENFILE_ACCESS_DIRECT        32 /* for direct IO */
#define OPENFILE_ACCESS_SYNC          64 /* for sync'ed IO */
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

enum FileInodeMode
{
   MODE_INVALID = -1,
   MODE_DEINLINE,
   MODE_REINLINE
};

template<>
struct SerializeAs<DirEntryType> {
   typedef uint8_t type;
};

inline std::ostream& operator<<(std::ostream& os, DirEntryType t) {
   switch (t) {
      case DirEntryType_DIRECTORY:   return os << "directory";
      case DirEntryType_REGULARFILE: return os << "file";
      case DirEntryType_SYMLINK:     return os << "symlink";
      case DirEntryType_BLOCKDEV:    return os << "block device node";
      case DirEntryType_CHARDEV:     return os << "character device node";
      case DirEntryType_FIFO:        return os << "pipe";
      case DirEntryType_SOCKET:      return os << "unix domain socket";

      default:
         return os << "invalid";
   }
}

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

typedef std::list<DirEntryType> DirEntryTypeList;
typedef DirEntryTypeList::iterator DirEntryTypeListIter;
typedef DirEntryTypeList::const_iterator DirEntryTypeListConstIter;

enum EntryLockRequestType
   {EntryLockRequestType_ENTRYFLOCK=0, EntryLockRequestType_RANGEFLOCK=1,
      EntryLockRequestType_ENTRYCOHERENCE=2, EntryLockRequestType_RANGECOHERENCE=3};


/**
 * Note: Typically used in combination with a value of SETATTR_CHANGE_...-Flags to determine which
 * of the fields are actually used
 */
struct SettableFileAttribs
{
   int32_t mode;
   uint32_t userID;
   uint32_t groupID;
   int64_t modificationTimeSecs; // unix mtime
   int64_t lastAccessTimeSecs;   // unix atime

   bool operator==(const SettableFileAttribs& other) const
   {
      return mode == other.mode
         && userID == other.userID
         && groupID == other.groupID
         && modificationTimeSecs == other.modificationTimeSecs
         && lastAccessTimeSecs == other.lastAccessTimeSecs;
   }

   bool operator!=(const SettableFileAttribs& other) const { return !(*this == other); }
};


// the TargetPathMap and the TargetFDMap is not required in StorageDefinitions.h of the
// fhgfs_client_module
typedef std::map<uint16_t, std::string> TargetPathMap; // keys: targetIDs, values: paths
typedef TargetPathMap::iterator TargetPathMapIter;
typedef TargetPathMap::const_iterator TargetPathMapCIter;
typedef TargetPathMap::value_type TargetPathMapVal;

// keys: targetIDs, values: file-descriptors
typedef std::map<uint16_t, int> TargetFDMap;
typedef TargetFDMap::iterator TargetFDMapIter;
typedef TargetFDMap::const_iterator TargetFDMapCIter;
typedef TargetFDMap::value_type TargetFDMapVal;


#endif /*STORAGEDEFINITIONS_H_*/
