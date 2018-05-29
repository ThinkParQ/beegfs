#ifndef __DEEPER_CACHE_H__
#define __DEEPER_CACHE_H__


#ifdef __cplusplus
extern "C" {
#endif


#include <dirent.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>


// API version defines
#define MAJOR_API_VERSION                1 // major version number of the API, different major
                                           // version are  incompatible
#define MINOR_API_VERSION                0 // minor version number of the API, the minor versions
                                           // of the same major version are backward compatible


// valid return values for cache functions
#define DEEPER_RETVAL_SUCCESS            0 // return value for success
#define DEEPER_RETVAL_ERROR             -1 // return value for an error

// additional return values for deeper_cache_[flush|prefetch]_is_finished functions
#define DEEPER_IS_NOT_FINISHED           0 // flush/prefetch is ongoing
#define DEEPER_IS_FINISHED               1 // flush/prefetch is finished

// valid deeper_open_flags
#define DEEPER_OPEN_NONE                 0 // no deeper open flags needed
#define DEEPER_OPEN_FLUSHWAIT            1 // wait until flush is finished on close (synchronous)
#define DEEPER_OPEN_FLUSHONCLOSE         2 // flush the deeper cache to global storage on close
#define DEEPER_OPEN_DISCARD              4 // delete file from deeper cache on close
#define DEEPER_OPEN_FLUSHFOLLOWSYMLINKS  8 // do not create symlinks, copy the destination file/dir

// valid deeper_prefetch_flags
#define DEEPER_PREFETCH_NONE             0 // no deeper prefetch flags needed
#define DEEPER_PREFETCH_WAIT             1 // wait until prefetch is finished (synchronous prefetch)
#define DEEPER_PREFETCH_SUBDIRS          2 // also prefetch all subdirectories
#define DEEPER_PREFETCH_FOLLOWSYMLINKS   4 // do not create symlinks, copy the destination file/dir

// valid deeper_flush_flags
#define DEEPER_FLUSH_NONE                0 // no deeper flush flags needed
#define DEEPER_FLUSH_WAIT                1 // wait until flush is finished (synchronous flush)
#define DEEPER_FLUSH_SUBDIRS             2 // also flush all subdirectories
#define DEEPER_FLUSH_DISCARD             4 // delete file from deeper cache after flush
#define DEEPER_FLUSH_FOLLOWSYMLINKS      8 // do not create symlinks, copy the destination file/dir



///////////////////////////////////////////////////////////////////////////////////////////////////
          /* BeeGFS extensions to access the partitioned DEEP-ER cache layer */
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Create a new directory on the cache layer of the current cache domain.
 *
 * @param path Path and name of new directory.
 * @param mode Permission bits, similar to POSIX mkdir (S_IRWXU etc.).
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int deeper_cache_mkdir(const char *path, mode_t mode);

/**
 * Remove a directory from the cache layer.
 *
 * @param path Path and name of directory, which should be removed.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int deeper_cache_rmdir(const char *path);

/**
 * Open a directory stream for a directory on the cache layer of the current cache domain and
 * position the stream at the first directory entry.
 *
 * @param name Path and name of directory on cache layer.
 * @return Pointer to directory stream, NULL and errno set in case of error.
 */
DIR *deeper_cache_opendir(const char *name);

/**
 * Close directory stream.
 *
 * @param dirp Directory stream, which should be closed.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int deeper_cache_closedir(DIR *dirp);

///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Open (and possibly create) a file on the cache layer.
 *
 * Any following write()/read() on the returned file descriptor will write/read data to/from the
 * cache layer. Data on the cache layer will be visible only to those nodes that are part of the
 * same cache domain.
 *
 * @param path Path and name of file, which should be opened.
 * @param oflag Access mode flags, similar to POSIX open (O_WRONLY, O_CREAT, etc).
 * @param mode The permissions of the file, similar to POSIX open. When oflag contains a file
 *        creation flag (O_CREAT, O_EXCL, O_NOCTTY, and O_TRUNC) the mode flag is required in other
 *        cases this flag is ignored.
 * @param deeper_open_flags DEEPER_OPEN_NONE or a combination of the following flags:
 *        DEEPER_OPEN_FLUSHONCLOSE to automatically flush written data to global
 *           storage when the file is closed, asynchronously.
 *        DEEPER_OPEN_FLUSHWAIT to make DEEPER_OPEN_FLUSHONCLOSE a synchronous operation, which
 *        means the close operation will only return after flushing is complete.
 *        DEEPER_OPEN_DISCARD to remove the file from the cache layer after it has been closed.
 * @return File descriptor as non-negative integer on success, DEEPER_RETVAL_ERROR and errno set
 *         in case of error.
 */
int deeper_cache_open(const char* path, int oflag, mode_t mode, int deeper_open_flags);

/**
 * Close a file.
 *
 * @param fildes File descriptor of open file.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int deeper_cache_close(int fildes);

///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Prefetch a file or directory (including contained files) from global storage to the current
 * cache domain of the cache layer, asynchronously.
 * Contents of existing files with the same name on the cache layer will be overwritten.
 *
 * @param path Path to file or directory, which should be prefetched.
 * @param deeper_prefetch_flags DEEPER_PREFETCH_NONE or a combination of the following flags:
 *        DEEPER_PREFETCH_SUBDIRS to recursively copy all subdirs, if given path leads to a
 *           directory.
 *        DEEPER_PREFETCH_WAIT To make this a synchronous operation.
 *        DEEPER_PREFETCH_FOLLOWSYMLINKS To copy the destination file or directory and do not
 *           create symbolic links when a symbolic link was found.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int deeper_cache_prefetch(const char* path, int deeper_prefetch_flags);

/**
 * Prefetch a file from global storage to the current cache domain of the cache layer,
 * asynchronously. A CRC checksum of the given file is calculated.
 * Contents of existing files with the same name on the cache layer will be overwritten.
 *
 * @param path Path to file or directory, which should be prefetched.
 * @param deeper_prefetch_flags DEEPER_PREFETCH_NONE or a combination of the following flags:
 *        DEEPER_PREFETCH_WAIT To make this a synchronous operation.
 *        DEEPER_PREFETCH_FOLLOWSYMLINKS To copy the destination file or directory and do not
 *           create symbolic links when a symbolic link was found.
 * @param outChecksum The checksum of the file, it is only used in the synchronous case.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int deeper_cache_prefetch_crc(const char* path, int deeper_prefetch_flags,
   unsigned long* outChecksum);

/**
 * Prefetch a file similar to deeper_cache_prefetch(), but prefetch only a certain range, not the
 * whole file.
 *
 * @param path Path to file, which should be prefetched.
 * @param pos Start position (offset) of the byte range that should be flushed.
 * @param num_bytes Number of bytes from pos that should be flushed.
 * @param deeper_prefetch_flags DEEPER_PREFETCH_NONE or the following flag:
  *        DEEPER_PREFETCH_WAIT to make this a synchronous operation.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int deeper_cache_prefetch_range(const char* path, off_t start_pos, size_t num_bytes,
   int deeper_prefetch_flags);

/**
 * Wait for an ongoing prefetch operation from deeper_cache_prefetch[_range]() to complete.
 *
 * @param path Path to file, which has been submitted for prefetch.
 * @param deeper_prefetch_flags DEEPER_PREFETCH_NONE or a combination of the following flags:
 *        DEEPER_PREFETCH_SUBDIRS To recursively wait contents of all subdirs, if given path leads
 *           to a directory.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int deeper_cache_prefetch_wait(const char* path, int deeper_prefetch_flags);

/**
 * Wait for an ongoing prefetch operation from deeper_cache_prefetch_crc() to complete.
 *
 * @param path Path to file, which has been submitted for prefetch.
 * @param deeper_prefetch_flags DEEPER_PREFETCH_NONE. Currently ignored, but maybe later some flags
 *           required.
 * @param outChecksum The checksum of the file.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int deeper_cache_prefetch_wait_crc(const char* path, int deeper_prefetch_flags,
   unsigned long* outChecksum);

/**
 * Checks if the prefetch of a the given path is finished. Checks the prefetch operation from
 * deeper_cache_prefetch[_range | _crc]().
 *
 * NOTE: The function doesn't report errors from the prefetch. To get the error is a additional wait
 *       required.
 *
 * @param path Path to file, which has been submitted for prefetch.
 * @param deeper_prefetch_flags The flags which was used for the prefetch.
 * @return DEEPER_IS_NOT_FINISHED If prefetch is ongoing or DEEPER_IS_FINISHED if prefetch is
 *         finished or DEEPER_RETVAL_ERROR in case of error.
 */
int deeper_cache_prefetch_is_finished(const char* path, int deeper_prefetch_flags);

/**
 * Stops an ongoing prefetch operation from deeper_cache_prefetch[_range | _crc]() to complete.
 *
 * @param path Path to file, which has been submitted for prefetch.
 * @param deeper_prefetch_flags The flags which was used for the prefetch.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int deeper_cache_prefetch_stop(const char* path, int deeper_prefetch_flags);

/**
 * Flush a file from the current cache domain to global storage, asynchronously. Contents of an
 * existing file with the same name on global storage will be overwritten.
 *
 * @param path Path to file, which should be flushed.
 * @param deeper_flush_flags DEEPER_FLUSH_NONE or a combination of the following flags:
 *        DEEPER_FLUSH_WAIT To make this a synchronous operation, which means return only after
 *           flushing is complete.
 *        DEEPER_FLUSH_SUBDIRS To recursively copy all subdirs, if given path leads to a
 *           directory.
 *        DEEPER_FLUSH_DISCARD To remove the file from the cache layer after it has been flushed.
 *        DEEPER_FLUSH_FOLLOWSYMLINKS To copy the destination file or directory and do not create
 *           symbolic links when a symbolic link was found.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int deeper_cache_flush(const char* path, int deeper_flush_flags);

/**
 * Flush a file from the current cache domain to global storage, asynchronously. A CRC checksum of
 * the given file is calculated.
 * Contents of an existing file with the same name on global storage will be overwritten.
 *
 * @param path Path to file, which should be flushed.
 * @param deeper_flush_flags DEEPER_FLUSH_NONE or a combination of the following flags:
 *        DEEPER_FLUSH_WAIT To make this a synchronous operation, which means return only after
 *           flushing is complete.
 *        DEEPER_FLUSH_DISCARD To remove the file from the cache layer after it has been flushed.
 *        DEEPER_FLUSH_FOLLOWSYMLINKS To copy the destination file or directory and do not create
 *           symbolic links when a symbolic link was found.
 * @param outChecksum The checksum of the file, it is only used in the synchronous case.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int deeper_cache_flush_crc(const char* path, int deeper_flush_flags, unsigned long* outChecksum);

/**
 * Flush a file similar to deeper_cache_flush(), but flush only a certain range, not the whole file.
 *
 * @param path Path to file, which should be flushed.
 * @param pos Start position (offset) of the byte range that should be flushed.
 * @param num_bytes Number of bytes from pos that should be flushed.
 * @param deeper_flush_flags DEEPER_FLUSH_NONE or a combination of the following flags:
 *        DEEPER_FLUSH_WAIT to make this a synchronous operation.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int deeper_cache_flush_range(const char* path, off_t start_pos, size_t num_bytes,
   int deeper_flush_flags);

/**
 * Wait for an ongoing flush operation from deeper_cache_flush[_range]() to complete.
 *
 * @param path Path to file, which has been submitted for flush.
 * @param deeper_flush_flags DEEPER_FLUSH_NONE or a combination of the following flags:
 *        DEEPER_FLUSH_SUBDIRS To recursively wait contents of all subdirs, if given path leads
 *           to a directory.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int deeper_cache_flush_wait(const char* path, int deeper_flush_flags);

/**
 * Wait for an ongoing flush operation from deeper_cache_flush_crc() to complete.
 *
 * @param path Path to file, which has been submitted for flush.
 * @param outChecksum The checksum of the file.
 * @param deeper_flush_flags DEEPER_FLUSH_NONE. Currently unused, but maybe later some flags
 *           required.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int deeper_cache_flush_wait_crc(const char* path, int deeper_flush_flags,
   unsigned long* outChecksum);

/**
 * Checks if the flush of a the given path is finished. Checks the flush operation from
 * deeper_cache_flush[_range | _crc]().
 *
 * NOTE: The function doesn't report errors from the flush. To get the error is a additional wait
 *       required.
 *
 * @param path Path to file, which has been submitted for flush.
 * @param deeper_flush_flags The flags which was used for the flush.
 * @return DEEPER_IS_NOT_FINISHED If flush is ongoing or DEEPER_IS_FINISHED if flush is finished or
 *         DEEPER_RETVAL_ERROR in case of error.
 */
int deeper_cache_flush_is_finished(const char* path, int deeper_flush_flags);

/**
 * Stops an ongoing flush operation from deeper_cache_flush[_range | _crc]() to complete.
 *
 * @param path Path to file, which has been submitted for flush.
 * @param deeper_flush_flags The flags which was used for the flush.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int deeper_cache_flush_stop(const char* path, int deeper_flush_flags);
///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Return the stat information of a file or directory of the cache domain.
 *
 * @param path To a file or directory on the global file system.
 * @param out_stat_data The stat information of the file or directory.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int deeper_cache_stat(const char *path, struct stat *out_stat_data);

///////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Return a unique identifier for the current cache domain.
 *
 * @param out_cache_id pointer To a buffer in which the ID of the current cache domain will be
 *        stored on success.
 * @return DEEPER_RETVAL_SUCCESS on success, DEEPER_RETVAL_ERROR and errno set in case of error.
 */
int deeper_cache_id(const char* path, uint64_t* out_cache_id);

/**
 * Checks if the required API version of the application is compatible to current API version.
 *
 * @param required_major_version The required major API version of the user application.
 * @param required_minor_version The minimal required minor API version of the user application.
 * @return DEEPER_RETVAL_SUCCESS if the required version and the API version are compatible, if not
 *         DEEPER_RETVAL_ERROR is returned.
 */
int deeper_cache_check_api_version(const unsigned required_major_version,
   const unsigned required_minor_version);

#ifdef __cplusplus
} // extern "C"
#endif


#endif /* __DEEPER_CACHE_H__ */
