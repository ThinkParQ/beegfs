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

   if(osFlags & O_NONBLOCK)
      fhgfsFlags |= OPENFILE_ACCESS_NONBLOCKING;


   return fhgfsFlags;
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
