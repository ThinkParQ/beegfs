#ifndef COMMON_CACHE_TOOLKIT_CACHEPATHTK_H_
#define COMMON_CACHE_TOOLKIT_CACHEPATHTK_H_

#include <common/app/log/Logger.h>
#include <common/cache/app/config/CacheConfig.h>
#include <common/Common.h>



namespace cachepathtk
{
   bool isRootPath(const std::string& path, const std::string& rootPath);
   bool isGlobalPath(const std::string& path, const CacheConfig* deeperLibConfig);
   bool isCachePath(const std::string& path, const CacheConfig* deeperLibConfig);
   bool globalPathToCachePath(std::string& inOutPath, const CacheConfig* deeperLibConfig,
      Logger* log);
   bool cachePathToGlobalPath(std::string& inOutPath, const CacheConfig* deeperLibConfig,
      Logger* log);

   bool makePathAbsolute(std::string& inOutPath, Logger* log);
   bool makePathAbsolute(std::string& inOutPath, const std::string& startDirOfPath, Logger* log);

   bool createPath(std::string destPath, std::string sourceMount, std::string destMount,
      bool excludeLastElement, Logger* log);
   bool createPath(const char* destPath, const std::string& sourceMount,
      const std::string& destMount, bool excludeLastElement, Logger* log);
   bool replaceRootPath(std::string& inOutPath, const std::string& origPath,
      const std::string& newPath, bool canonicalizePaths, bool convertToCachePath, Logger* log);

   bool readLink(const char* sourcePath, const struct stat* statSource, Logger* log,
      std::string& outLinkContent);

   void preparePaths(std::string& inOutString);

   bool bothCharactersAreSlashes(char a, char b);
};

#endif /* COMMON_CACHE_TOOLKIT_CACHEPATHTK_H_ */
