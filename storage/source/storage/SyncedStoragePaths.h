#ifndef SYNCEDSTORAGEPATHS_H_
#define SYNCEDSTORAGEPATHS_H_

#include <common/app/log/LogContext.h>
#include <common/system/System.h>
#include <common/threading/Condition.h>
#include <common/Common.h>

#include <mutex>

class SyncedStoragePaths
{
   // type definitions
   typedef std::set<std::string> StoragePathsSet; // key: string (path and target)
   typedef StoragePathsSet::iterator StoragePathsSetIter;
   typedef StoragePathsSet::value_type StoragePathsSetVal;


   public:
      SyncedStoragePaths()
      {
         initStorageVersion();
      }


   private:
      Mutex mutex;
      Condition eraseCond; // broadcasted when path erased from map
      uint64_t storageVersion; // zero is the invalid version!
      StoragePathsSet paths; // for currently locked paths


      // inliners

      void initStorageVersion()
      {
         /* note: we assume here that the clock doesn't jump too much backwards between restarts of
            the daemon and that we don't have more than 2^32 increase ops per second (the latter
            shouldn't be a problem for the next years) */

         uint64_t currentSecs = System::getCurrentTimeSecs();

         this->storageVersion = (currentSecs << 32);
      }

      /**
       * Note: Caller must hold the mutex.
       */
      uint64_t incStorageVersion()
      {
         return ++storageVersion;
      }


   public:
      // inliners

      /**
       * Locks a path and creates a new monotonic increasing storage version for it.
       *
       * Note: Make sure to unlock the same path later via unlockPath().
       *
       * @return storage version for this path lock
       */
      uint64_t lockPath(const std::string path, uint16_t targetID)
      {
         /* we just have to make sure that each target+path is inserted (=>locked) only once and
            that the next one who wants to insert the same path will wait until the old path is
            erased (=> unlocked) */

         std::string targetPath(path + "@" + StringTk::uint64ToHexStr(targetID) );

         const std::lock_guard<Mutex> lock(mutex);

         while(!paths.insert(targetPath).second)
            eraseCond.wait(&mutex);

         return incStorageVersion();
      }

      void unlockPath(const std::string path, uint16_t targetID)
      {
         // unlocking just means we erase the target+path from the map, so the next one can lock it

         std::string targetPath(path + "@" + StringTk::uintToHexStr(targetID) );

         const std::lock_guard<Mutex> lock(mutex);

         size_t numErased = paths.erase(targetPath);
         if(unlikely(!numErased) )
         {
            LOG_DEBUG("SyncedStorgePaths::unlockPath", Log_ERR,
               "Attempt to unlock a path that wasn't locked: " + targetPath);
         }

         eraseCond.broadcast();
      }

};


#endif /* SYNCEDSTORAGEPATHS_H_ */
