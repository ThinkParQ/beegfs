#ifndef BUDDYRESYNCER_H_
#define BUDDYRESYNCER_H_

#include <components/buddyresyncer/BuddyResyncJob.h>

#include <mutex>

/**
 * This is not a component that represents a separate thread by itself. Instead, it is the
 * controlling frontend for slave threads, which are started and stopped on request (i.e. it is not
 * automatically started when the app is started).
 *
 * Callers should only use methods in this controlling frontend and not access the slave's methods
 * directly.
 */
class BuddyResyncer
{
   public:
      ~BuddyResyncer();

      FhgfsOpsErr startResync(uint16_t targetID);

   private:
      BuddyResyncJobMap resyncJobMap;
      Mutex resyncJobMapMutex;

   public:
      BuddyResyncJob* getResyncJob(uint16_t targetID)
      {
         std::lock_guard<Mutex> mutexLock(resyncJobMapMutex);

         BuddyResyncJobMapIter iter = resyncJobMap.find(targetID);
         if (iter != resyncJobMap.end())
            return iter->second;
         else
            return NULL;
      }

   private:
      BuddyResyncJob* addResyncJob(uint16_t targetID, bool& outIsNew)
      {

         std::lock_guard<Mutex> mutexLock(resyncJobMapMutex);

         BuddyResyncJobMapIter iter = resyncJobMap.find(targetID);
         if (iter != resyncJobMap.end())
         {
            outIsNew = false;
            return iter->second;
         }
         else
         {
            BuddyResyncJob* job = new BuddyResyncJob(targetID);
            resyncJobMap.insert(BuddyResyncJobMap::value_type(targetID, job) );
            outIsNew = true;
            return job;
         }
      }
};

#endif /* BUDDYRESYNCER_H_ */
