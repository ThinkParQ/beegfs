#ifndef FILESYSTEM_DEEPERCACHE_H_
#define FILESYSTEM_DEEPERCACHE_H_


#include <app/App.h>
#include <app/config/Config.h>
#include <common/threading/Mutex.h>
#include <common/app/log/Logger.h>
#include <common/Common.h>
#include <common/nodes/LocalNode.h>
#include <common/nodes/NodeConnPool.h>
#include <session/DeeperCacheSession.h>

#include <dirent.h>


typedef std::map<int, DeeperCacheSession> DeeperCacheSessionMap;
typedef DeeperCacheSessionMap::iterator DeeperCacheSessionMapIter;
typedef DeeperCacheSessionMap::value_type DeeperCacheSessionMapVal;


class DeeperCache
{
   public:
      int cache_mkdir(const char *path, mode_t mode);
      int cache_rmdir(const char *path);

      DIR* cache_opendir(const char *name);
      int cache_closedir(DIR *dirp);

      int cache_open(const char* path, int oflag, mode_t mode, int deeper_open_flags);
      int cache_close(int fildes);

      int cache_prefetch(const char* path, int deeper_prefetch_flags);
      int cache_prefetch_crc(const char* path, int deeper_prefetch_flags,
         unsigned long* outChecksum);
      int cache_prefetch_range(const char* path, off_t start_pos, size_t num_bytes,
         int deeper_prefetch_flags);
      int cache_prefetch_wait(const char* path, int deeper_prefetch_flags);
      int cache_prefetch_wait_crc(const char* path, int deeper_prefetch_flags,
         unsigned long* outChecksum);
      int cache_prefetch_is_finished(const char* path, int deeper_prefetch_flags);
      int cache_prefetch_stop(const char* path, int deeper_prefetch_flags);

      int cache_flush(const char* path, int deeper_flush_flags);
      int cache_flush_crc(const char* path, int deeper_flush_flags, unsigned long* outChecksum);
      int cache_flush_range(const char* path, off_t start_pos, size_t num_bytes,
         int deeper_flush_flags);
      int cache_flush_wait(const char* path, int deeper_flush_flags);
      int cache_flush_wait_crc(const char* path, int deeper_flush_flags,
         unsigned long* outChecksum);
      int cache_flush_is_finished(const char* path, int deeper_flush_flags);
      int cache_flush_stop(const char* path, int deeper_flush_flags);

      int cache_stat(const char *path, struct stat *out_stat_data);

      int cache_id(const char* path, uint64_t* out_cache_id);


   private:
      DeeperCache();
      ~DeeperCache();
      DeeperCache(DeeperCache const&);       // not allowed for a singelton
      void operator=(DeeperCache const&);    // not allowed for a singelton

      static App* app;

      Mutex fdMapMutex;
      DeeperCacheSessionMap sessionMap;

      Mutex prefetchMapMutex;
      StringList prefetchList;

      Mutex flushMapMutex;
      StringList flushList;

      std::shared_ptr<Node> localNode;

      static DeeperCache* cacheInstance;

      void addEntryToSessionMap(int fd, int deeper_open_flags, std::string path);
      void getAndRemoveEntryFromSessionMap(DeeperCacheSession& inOutSession);


   public:
      static DeeperCache* getInstance();
      static void cleanup();

      App* getApp()
      {
         return app;
      }

      Config* getConfig()
      {
         return app->getConfig();
      }

      Logger* getLogger()
      {
         return Logger::getLogger();
      }

      Node& getLocalNode() const
      {
         return *localNode;
      }
};

#endif /* FILESYSTEM_DEEPERCACHE_H_ */
