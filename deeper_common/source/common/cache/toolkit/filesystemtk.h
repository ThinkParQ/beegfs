#ifndef COMMON_CACHE_TOOLKIT_FILESYSTEMTK_H_
#define COMMON_CACHE_TOOLKIT_FILESYSTEMTK_H_


#include <common/cache/app/config/CacheConfig.h>
#include <common/cache/components/worker/CacheWork.h>

#include <ftw.h>
#include <limits.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>



namespace filesystemtk
{
   const unsigned NFTW_MAX_OPEN_FD        = 100;               // max open file descriptors for NFTW
   const size_t BUFFER_SIZE               = (1024 * 1024 * 2); // buffer size 2 MB
   const int MAX_NUM_FOLLOWED_SYMLINKS    = 20;                // max number of symlinks to follow


   int copyFile(const char* sourcePath, const char* destPath, const struct stat* statSource,
      bool deleteSource, bool doCRC, CacheWork* work, unsigned long* crcOutValue);
   int copyFileRange(const char* sourcePath, const char* destPath, const struct stat* statSource,
      off_t* offset, size_t numBytes, CacheWork* work, bool doChown);
   int copyDir(const char* source, const char* dest, bool copyToCache, bool deleteSource,
      bool followSymlink);
   int createSymlink(const char* sourcePath, const char* destPath, const struct stat* statSource,
      bool copyToCache, bool deleteSource);
   int handleSubdirectoryFlag(const char* source, const char* dest, bool copyToCache,
      bool deleteSource, bool followSymlink);
   int handleFollowSymlink(std::string sourcePath, std::string destPath,
      const struct stat* statSourceSymlink, bool copyToCache, bool deleteSource, bool ignoreDir,
      bool doCRC, unsigned long* crcOutValue);


   int nftwCopyFileToCache(const char *fpath, const struct stat *sb, int tflag,
      struct ::FTW *ftwbuf);
   int nftwCopyFileToGlobal(const char *fpath, const struct stat *sb, int tflag,
      struct ::FTW *ftwbuf);
   int nftwHandleFlags(std::string sourcePath, std::string destPath, const struct stat *sb,
      int tflag, bool copyToCache, bool deleteSource, bool followSymlink);
   int nftwDelete(const char *fpath, const struct stat *sb, int tflag, struct ::FTW *ftwbuf);
} /* namespace FileSystemTk */

#endif /* COMMON_CACHE_TOOLKIT_FILESYSTEMTK_H_ */
