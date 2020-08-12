#ifndef MSGHELPERIO_H_
#define MSGHELPERIO_H_

#include <app/config/Config.h>
#include <app/App.h>
#include <common/Common.h>
#include <program/Program.h>

/* The kernel has a weird read-ahead size limitation, but from userspace only. If this ever will
 * be abondoned, we need to make a config option for those future kernels. */
#define MAX_KERNEL_READAHEAD (2 * 1024 * 1024)


#if ( (_XOPEN_SOURCE >= 700 && _POSIX_C_SOURCE >= 200809L) && (defined (AT_SYMLINK_NOFOLLOW) ) )
        #define MsgHelperIO_USE_UTIMENSAT
#endif

// positions in struct timeval and struct timespec
#define MsgHelperIO_ATIME_POS 0
#define MsgHelperIO_MTIME_POS 1

#ifndef UTIME_OMIT
#define UTIME_OMIT   ((1l << 30) - 2l)
#endif


/**
 * Wrappers for basic IO syscalls.
 *
 * This class has no really good reason to exist since we do no longer have the SingleIOWorker,
 * which was based on not directly calling the syscalls from the current thread.
 * However, it's still nice to have this central place for IO routines and since it doesn't impose
 * any overhead, we just keep it for now.
 */
class MsgHelperIO
{
   public:
      
      
   private:
      MsgHelperIO() {}
      
      
   public:
      // inliners
      static int open(const char* pathname, int flags, mode_t mode)
      {
         return ::open(pathname, flags, mode);
      }

      static int openat(const int dirFD, const char* pathname, int flags, mode_t mode)
      {
         return ::openat(dirFD, pathname, flags, mode);
      }

      static int close(int fd)
      {
         return ::close(fd);
      }
      
      static ssize_t read(int fd, void* buf, size_t count)
      {
         return ::read(fd, buf, count);
      }

      static ssize_t pread(int fd, void* buf, size_t count, off_t offset)
      {
         return ::pread(fd, buf, count, offset);
      }

      static ssize_t write(int fd, const void* buf, size_t count)
      {
         return ::write(fd, buf, count);
      }

      static ssize_t pwrite(int fd, const void* buf, size_t count, off_t offset)
      {
         return ::pwrite(fd, buf, count, offset);
      }


      static off_t lseek(int fd, off_t offset, int whence)
      {
         return ::lseek(fd, offset, whence);
      }
      
      static int fsync(int fd)
      {
         return ::fsync(fd);
      }

      /**
       * Sync a given range of the given fd.
       *
       * Note: This is a linux specific call, which appeared in linux-2.6.17 and glibc 2.6
       * (so it is not available in SLES10/RHEL5).
       */
      static int syncFileRange(int fd, off64_t offset, off64_t nbytes)
      {
         #ifdef CONFIG_DISTRO_HAS_SYNC_FILE_RANGE
            return sync_file_range(fd, offset, nbytes, SYNC_FILE_RANGE_WRITE);
         #else
            return 0;
         #endif
      }

      /**
       * Advise the kernel to read-ahead the given amount of data. Especially for block based
       * file systems this is an asynchronous call one the IO has reached the bio layer.
       */
      static int readAhead(int fd, off_t offset, off_t remainingLen)
      {
         int raRes = 0;

         while (remainingLen > 0)
         {
            size_t readSize = BEEGFS_MIN(MAX_KERNEL_READAHEAD, remainingLen);

            raRes = posix_fadvise(fd, offset, readSize, POSIX_FADV_WILLNEED);
            if (unlikely(raRes) )
               break;

            offset += readSize;
            remainingLen -= readSize;

         }

         return raRes;
      }

      /**
       * Simulate non-existing truncateat()
       */
      static int truncateAt(const int dirFD, const char *path, const off_t length)
      {
         int fd = ::openat(dirFD, path, O_WRONLY);
         if (fd == -1)
            return -1;

         int truncRes = ::ftruncate(fd, length);

         int closeRes = ::close(fd);

         int retVal = 0;

         if (truncRes == -1 || closeRes == -1)
            retVal = -1;

         return retVal;
      }

      static int utimensat(int dirFD, const char *path, const struct timespec timeSpec[2],
         int flags)
      {
#ifdef MsgHelperIO_USE_UTIMENSAT
         return ::utimensat(dirFD, path, timeSpec, flags);
#else
         struct timeval timeVal[2];

         /* note: Below we rely on the fact that at least either atime or mtime are to be set.
          *       futimesat() is an intermediate linux only call, deprecated by utimensat, but
          *       it is available on SLES10 and RHEL5, so we can use it for these old platforms
          *       where utimensat() was not available yet.  */

         if (timeSpec[MsgHelperIO_ATIME_POS].tv_nsec == UTIME_OMIT)
         {  /* atime is not supposed to be set, correct would be to stat the current atime and then
             * to set this value here. For now we simply set the mtime */
            timeVal[MsgHelperIO_ATIME_POS].tv_sec  = timeSpec[MsgHelperIO_MTIME_POS].tv_sec;
            timeVal[MsgHelperIO_ATIME_POS].tv_usec = timeSpec[MsgHelperIO_MTIME_POS].tv_nsec / 1000;
         }
         else
         {  // client request an atime update
            timeVal[MsgHelperIO_ATIME_POS].tv_sec  = timeSpec[MsgHelperIO_ATIME_POS].tv_sec;
            timeVal[MsgHelperIO_ATIME_POS].tv_usec = timeSpec[MsgHelperIO_ATIME_POS].tv_nsec / 1000;
         }

         if (timeSpec[MsgHelperIO_MTIME_POS].tv_nsec == UTIME_OMIT)
         {  /* similar to atime above, but this time mtime is not supposed to be set, we use the
             * the atime value instead */
            timeVal[MsgHelperIO_MTIME_POS].tv_sec  = timeSpec[MsgHelperIO_ATIME_POS].tv_sec;
            timeVal[MsgHelperIO_MTIME_POS].tv_usec = timeSpec[MsgHelperIO_ATIME_POS].tv_nsec / 1000;
         }
         else
         {  // client request an mtime update
            timeVal[MsgHelperIO_MTIME_POS].tv_sec  = timeSpec[MsgHelperIO_MTIME_POS].tv_sec;
            timeVal[MsgHelperIO_MTIME_POS].tv_usec = timeSpec[MsgHelperIO_MTIME_POS].tv_nsec / 1000;
         }

         return ::futimesat(dirFD, path, timeVal);

#endif
      }
};

#endif /* MSGHELPERIO_H_ */
