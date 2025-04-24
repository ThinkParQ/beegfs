#pragma once

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "pmq_logging.hpp"

/* Wrapper around open() to open directories to put this awkward code in a
 * central place.  It is counter-intuitive but this is apparently how you're
 * supposed to open directories on Unix, both the reading and "writing" (i.e.
 * create, unlink, rename).
 * The O_DIRECTORY flag is optional but the O_RDONLY is not; opening with
 * O_RDWR | O_DIRECTORY fails with "is a directory" (weird!).
 *
 * Returns: fd to open directory or -1, in which case the errno variable must
 * be handled as usual.
 */
static inline int pmq_open_dir(const char *path)
{
   return open(path, O_RDONLY | O_DIRECTORY);
}

static inline int pmq_check_regular_file(int fd, const char *what_file)
{
   struct stat st;

   if (fstat(fd, &st) == -1)
   {
      pmq_perr_ef(errno, "Failed to fstat() the fd we opened");
      return false;
   }

   if (! S_ISREG(st.st_mode))
   {
      pmq_perr_f("We opened the file '%s' expecting a regular file but it's not",
            what_file);
      return false;
   }

   return true;
}

// Note: returns an fd >= 0 if successful.
// On failure, -1 is returned and
//  - if the file failed to open, errno indicates why the file failed to open.
//  - if the file was opened successfuly but then closed again because it was not
//    a regular file, errno is set to 0.
static inline int pmq_openat_regular_existing(
      int basedir_fd, const char *relpath, int flags)
{
   // only access mode may be specified -- no other flags
   // In particular, O_CREAT would break the logic.
   assert(flags == O_RDWR || flags == O_RDONLY || flags == O_WRONLY);

   int fd = openat(basedir_fd, relpath, flags, 0);

   if (fd == -1)
   {
      int e = errno;
      pmq_perr_ef(errno, "Failed to openat() existing file='%s', flags=%x",
            relpath, flags);
      errno = e;
      return fd;
   }

   /* The case where fd refers to something other than a regular file _may_
    * have been caught by the kernel already above. For example, opening a
    * directory using O_RDWR will fail. On the other hand, opening a directory
    * using O_RDONLY will succeed.
    * In any case, doing an explicit check here.
    */
   if (! pmq_check_regular_file(fd, relpath))
   {
      close(fd);
      errno = 0;
      return -1;
   }

   return fd;
}

static inline int pmq_openat_regular_create(
      int basedir_fd, const char *relpath, int flags, mode_t mode)
{
   // only access mode may be specified -- no other flags
   assert(flags == O_RDWR || flags == O_RDONLY || flags == O_WRONLY);

   // But this func makes sure that the creation-flags are specified
   flags |= O_CREAT | O_EXCL;

   int fd = openat(basedir_fd, relpath, flags, mode);

   if (fd == -1)
   {
      int e = errno;
      pmq_perr_ef(errno, "Failed to openat() file='%s', flags=%x, mode=%o",
            relpath, flags, (unsigned) mode);
      errno = e;
      return fd;
   }

   // all necessarily error handling should be done by the OS. (note O_EXCL)

   return fd;
}

__pmq_artificial_func
void assert_sane_size(size_t size)
{
   // check that size is representable as a ssize_t too.
   // It is implementation defined how syscalls like write() handle write I/O sizes larger than SSIZE_T.
   // So better don't even try to.

   assert((size_t) (ssize_t) size == size);
}

static inline bool pmq_write_all(int fd, Untyped_Slice slice, const char *what)
{
   assert_sane_size(slice.size());

   while (slice.size())
   {
      ssize_t nw = write(fd, slice.data(), slice.size());

      if (nw == -1)
      {
         int e = errno;
         pmq_perr_ef(errno, "Failed to write %zu bytes to %s",
               slice.size(), what);
         errno = e;
         return false;
      }

      slice = slice.offset_bytes((size_t) nw);
   }

   return true;
}

static inline bool pmq_pwrite_all(int fd, Untyped_Slice slice, off_t offset, const char *what)
{
   assert_sane_size(slice.size());

   while (slice.size())
   {
      ssize_t nw = pwrite(fd, slice.data(), slice.size(), offset);

      if (nw == -1)
      {
         int e = errno;
         pmq_perr_ef(errno, "Failed to pwrite() %zu bytes at offset %jd to %s",
               slice.size(), (intmax_t) offset, what);
         errno = e;
         return false;
      }

      slice = slice.offset_bytes((size_t) nw);
      offset += (size_t) nw;
   }

   return true;
}

static inline bool pmq_read_all(int fd, Untyped_Slice slice, const char *what)
{
   assert_sane_size(slice.size());

   while (slice.size())
   {
      ssize_t nw = read(fd, slice.data(), slice.size());

      if (nw == -1)
      {
         int e = errno;
         pmq_perr_ef(errno, "Failed to read %zu bytes from %s",
               slice.size(), what);
         errno = e;
         return false;
      }

      slice = slice.offset_bytes((size_t) nw);
   }

   return true;
}

static inline bool pmq_pread_all(int fd, Untyped_Slice slice, off_t offset, const char *what)
{
   assert_sane_size(slice.size());

   while (slice.size())
   {
      ssize_t nw = pread(fd, slice.data(), slice.size(), offset);

      if (nw == -1)
      {
         int e = errno;
         pmq_perr_ef(errno, "Failed to pread() %zu bytes at offset %jd to %s",
               slice.size(), (intmax_t) offset, what);
         errno = e;
         return false;
      }

      slice = slice.offset_bytes((size_t) nw);
      offset += (size_t) nw;
   }

   return true;
}
