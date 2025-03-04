#ifndef OSTYPECONVERSION_INTERNAL_H_
#define OSTYPECONVERSION_INTERNAL_H_

#include <common/Common.h>
#include <os/OsTypeConversion.h>
#include <common/toolkit/Time.h>
#include <common/storage/StorageDefinitions.h>

#include <linux/fs.h>
#if defined(KERNEL_HAS_LINUX_FILELOCK_H)
#include <linux/filelock.h>
#endif

static inline int OsTypeConv_openFlagsOsToFhgfs(int osFlags, bool isPagedMode);
static inline void OsTypeConv_kstatFhgfsToOs(fhgfs_stat* fhgfsStat, struct kstat* kStat);
static inline void OsTypeConv_iattrOsToFhgfs(struct iattr* iAttr, SettableFileAttribs* fhgfsAttr,
   int* outValidAttribs);
static inline unsigned OsTypeConv_dirEntryTypeToOS(DirEntryType entryType);
static inline int OsTypeConv_flockTypeToFhgfs(struct file_lock* fileLock);


/**
 * @param osFlags file open mode flags
 * @return OPENFILE_ACCESS_... flags
 */
int OsTypeConv_openFlagsOsToFhgfs(int osFlags, bool isPagedMode)
{
   int fhgfsFlags = 0;

   if(osFlags & O_RDWR)
      fhgfsFlags |= OPENFILE_ACCESS_READWRITE;
   else
   if(osFlags & O_WRONLY)
   {
      if (!isPagedMode)
         fhgfsFlags |= OPENFILE_ACCESS_WRITE;
      else
      {  /* in order to update read-modify-write pages with the storage content we a
          * read-write handle */
         fhgfsFlags |= OPENFILE_ACCESS_READWRITE;
      }
   }
   else
      fhgfsFlags |= OPENFILE_ACCESS_READ;


   if(osFlags & O_APPEND)
      fhgfsFlags |= OPENFILE_ACCESS_APPEND;

   if(osFlags & O_TRUNC)
      fhgfsFlags |= OPENFILE_ACCESS_TRUNC;

   if(osFlags & O_DIRECT)
      fhgfsFlags |= OPENFILE_ACCESS_DIRECT;

   if(osFlags & O_SYNC)
      fhgfsFlags |= OPENFILE_ACCESS_SYNC;


   return fhgfsFlags;
}

/**
 * @param kStat unused fields will be set to zero
 */
void OsTypeConv_kstatFhgfsToOs(fhgfs_stat* fhgfsStat, struct kstat* kStat)
{
   memset(kStat, 0, sizeof(*kStat) );

   kStat->mode = fhgfsStat->mode;
   kStat->nlink = fhgfsStat->nlink;
   kStat->uid = make_kuid(&init_user_ns, fhgfsStat->uid);
   kStat->gid = make_kgid(&init_user_ns, fhgfsStat->gid);
   kStat->size = fhgfsStat->size;
   kStat->blocks = fhgfsStat->blocks;
   kStat->atime.tv_sec = fhgfsStat->atime.tv_sec;
   kStat->atime.tv_nsec = fhgfsStat->atime.tv_nsec;
   kStat->mtime.tv_sec = fhgfsStat->mtime.tv_sec;
   kStat->mtime.tv_nsec = fhgfsStat->mtime.tv_nsec;
   kStat->ctime.tv_sec = fhgfsStat->ctime.tv_sec; // attrib change time (not creation time)
   kStat->ctime.tv_nsec = fhgfsStat->ctime.tv_nsec; // attrib change time (not creation time)
}

/**
 * Convert kernel iattr to fhgfsAttr. Also update the inode with the new attributes.
 */
void OsTypeConv_iattrOsToFhgfs(struct iattr* iAttr, SettableFileAttribs* fhgfsAttr,
   int* outValidAttribs)
{
   Time now;
   Time_setToNowReal(&now);

   *outValidAttribs = 0;

   if(iAttr->ia_valid & ATTR_MODE)
   {
      (*outValidAttribs) |= SETATTR_CHANGE_MODE;
      fhgfsAttr->mode  = iAttr->ia_mode;
   }

   if(iAttr->ia_valid & ATTR_UID)
   {
      (*outValidAttribs) |= SETATTR_CHANGE_USERID;
      fhgfsAttr->userID = from_kuid(&init_user_ns, iAttr->ia_uid);
   }

   if(iAttr->ia_valid & ATTR_GID)
   {
      (*outValidAttribs) |= SETATTR_CHANGE_GROUPID;
      fhgfsAttr->groupID = from_kgid(&init_user_ns, iAttr->ia_gid);
   }

   if(iAttr->ia_valid & ATTR_MTIME_SET)
   {
      (*outValidAttribs) |= SETATTR_CHANGE_MODIFICATIONTIME;
      fhgfsAttr->modificationTimeSecs = iAttr->ia_mtime.tv_sec;
   }
   else
   if(iAttr->ia_valid & ATTR_MTIME)
   { // set mtime to "now"
      (*outValidAttribs) |= SETATTR_CHANGE_MODIFICATIONTIME;
      fhgfsAttr->modificationTimeSecs = now.tv_sec;
   }

   if(iAttr->ia_valid & ATTR_ATIME_SET)
   {
      (*outValidAttribs) |= SETATTR_CHANGE_LASTACCESSTIME;
      fhgfsAttr->lastAccessTimeSecs = iAttr->ia_atime.tv_sec;
   }
   else
   if(iAttr->ia_valid & ATTR_ATIME)
   { // set atime to "now"
      (*outValidAttribs) |= SETATTR_CHANGE_LASTACCESSTIME;
      fhgfsAttr->lastAccessTimeSecs = now.tv_sec;
   }
}

/**
 * Convert fhgfs DirEntryType to OS DT_... for readdir()'s filldir.
 */
unsigned OsTypeConv_dirEntryTypeToOS(DirEntryType entryType)
{
   if(DirEntryType_ISDIR(entryType) )
      return DT_DIR;

   if(DirEntryType_ISREGULARFILE(entryType) )
      return DT_REG;

   if(DirEntryType_ISSYMLINK(entryType) )
      return DT_LNK;

   if(DirEntryType_ISBLOCKDEV(entryType) )
      return DT_BLK;

   if(DirEntryType_ISCHARDEV(entryType) )
      return DT_CHR;

   if(DirEntryType_ISFIFO(entryType) )
      return DT_FIFO;

   if(DirEntryType_ISSOCKET(entryType) )
      return DT_SOCK;

   return DT_UNKNOWN;
}

/**
 * Convert the OS F_..LCK lock type flags of an flock operation to fhgfs ENTRYLOCKTYPE_... lock type
 * flags.
 *
 * @
 */
static inline int OsTypeConv_flockTypeToFhgfs(struct file_lock* fileLock)
{
   int fhgfsLockFlags = 0;

   switch(FhgfsCommon_getFileLockType(fileLock))
   {
      case F_RDLCK:
      {
         fhgfsLockFlags = ENTRYLOCKTYPE_SHARED;
      } break;

      case F_WRLCK:
      {
         fhgfsLockFlags = ENTRYLOCKTYPE_EXCLUSIVE;
      } break;

      default:
      {
         fhgfsLockFlags = ENTRYLOCKTYPE_UNLOCK;
      } break;
   }

   if(!(FhgfsCommon_getFileLockFlags(fileLock) & FL_SLEEP) )
      fhgfsLockFlags |= ENTRYLOCKTYPE_NOWAIT;

   return fhgfsLockFlags;
}

#endif /* OSTYPECONVERSION_INTERNAL_H_ */
