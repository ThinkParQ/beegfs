#ifndef TOOLKIT_ASYNCFILESYSTEMTK_H_
#define TOOLKIT_ASYNCFILESYSTEMTK_H_


#include <common/cache/components/worker/CacheWork.h>



namespace asyncfilesystemtk
{
   int copyDir(const char* source, const char* dest, int deeperFlags, CacheWorkType type,
      CacheWork* rootWork, int followSymlinkCounter);
   int handleSubdirectoryFlag(const char* source, const char* dest, int deeperFlags,
      CacheWorkType type, CacheWork* rootWork, bool isRootWork, int followSymlinkCounter);
   int handleFollowSymlink(std::string sourcePath, std::string destPath,
      const struct stat* statSourceSymlink, bool copyToCache, bool deleteSource, bool ignoreDir,
      bool doCRC, int deeperFlags, CacheWorkType type, CacheWork* rootWork,
      int followSymlinkCounter, unsigned long* crcOutValue);
   int doDelete(const char* sourcePath, int deeperFlags, CacheWork* rootWork, bool rootWorkIsLocked,
      bool &outSubdirDeleted);

   int calculateNumCopyThreads(uint64_t minBlockSize, uint64_t fileSize);
} /* namespace asyncfilesystemtk */

#endif /* TOOLKIT_ASYNCFILESYSTEMTK_H_ */
