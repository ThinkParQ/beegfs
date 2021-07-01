#include <common/threading/PThread.h>
#include <common/toolkit/Random.h>
#include <session/EntryLockStore.h>

#include <gtest/gtest.h>

class ParentNameLockTestThread : public PThread
{
   public:
      ParentNameLockTestThread(const unsigned id, EntryLockStore& entryLockStore,
         const std::string& parentID, const std::string& name):
         PThread("ParentNameLockTestThread" + StringTk::uintToStr(id)),
         entryLockStore(entryLockStore), parentID(parentID), name(name)
      {
      }

      void run() override
      {
         const unsigned maxSleepSeconds = 1;

         const std::string threadName = getCurrentThreadName();
         const char* logContext(threadName.c_str());

         Random r;
         const int sleepSec = r.getNextInRange(0, maxSleepSeconds);

         LogContext(logContext).log(Log_DEBUG, "Trying to lock '" + parentID + "/" + name + "'");

         ParentNameLockData* lock = entryLockStore.lock(parentID, name);

         LogContext(logContext).log(Log_DEBUG, "Locked '" + parentID + "/" + name + "'");
         LogContext(logContext).log(Log_DEBUG, "Sleeping for " + StringTk::intToStr(sleepSec) +
            " seconds");

         sleep(sleepSec);

         LogContext(logContext).log(Log_DEBUG, "Trying to unlock '" + parentID + "/" + name + "'");

         entryLockStore.unlock(lock);

         LogContext(logContext).log(Log_DEBUG, "Unlocked '" + parentID + "/" + name + "'");
      }

   private:
      EntryLockStore& entryLockStore;
      const std::string parentID;
      const std::string name;
};

class FileIDLockTestThread : public PThread
{
   public:
      FileIDLockTestThread(const unsigned id, EntryLockStore& entryLockStore,
         const std::string& fileID) :
         PThread("FileIDLockTestThread" + StringTk::uintToStr(id)), entryLockStore(entryLockStore),
         fileID(fileID)
      {
      }

      void run() override
      {
         const unsigned maxSleepSeconds = 1;

         const std::string threadName = getCurrentThreadName();
         const char* logContext(threadName.c_str());

         Random r;
         const int sleepSec = r.getNextInRange(0, maxSleepSeconds);

         LogContext(logContext).log(Log_DEBUG, "Trying to lock '" + fileID + "'");

         FileIDLockData* lock = entryLockStore.lock(fileID, true);

         LogContext(logContext).log(Log_DEBUG, "Locked '" + fileID + "'");
         LogContext(logContext).log(Log_DEBUG, "Sleeping for " + StringTk::intToStr(sleepSec) +
            " seconds");

         sleep(sleepSec);

         LogContext(logContext).log(Log_DEBUG, "Trying to unlock '" + fileID + "'");

         entryLockStore.unlock(lock);

         LogContext(logContext).log(Log_DEBUG, "Unlocked '" + fileID + "'");
      }

   private:
      EntryLockStore& entryLockStore;
      const std::string fileID;
};

class DirIDLockTestThread : public PThread
{
   public:
      DirIDLockTestThread(const unsigned id, EntryLockStore& entryLockStore,
         const std::string& dirID, const bool writeLock) :
         PThread("DirIDLockTestThread" + StringTk::uintToStr(id)), entryLockStore(entryLockStore),
         dirID(dirID), writeLock(writeLock)
      {
      }

      void run() override
      {
         const unsigned maxSleepSeconds = 1;
         const std::string logInfo(writeLock ? dirID + "/write" : dirID + "/read");
         const std::string threadName = getCurrentThreadName();
         const char* logContext(threadName.c_str());

         Random r;
         const int sleepSec = r.getNextInRange(0, maxSleepSeconds);

         LogContext(logContext).log(Log_DEBUG, "Trying to lock '" + logInfo + "'");

         FileIDLockData* lock = entryLockStore.lock(dirID, writeLock);

         LogContext(logContext).log(Log_DEBUG, "Locked '" + logInfo + "'");
         LogContext(logContext).log(Log_DEBUG, "Sleeping for " + StringTk::intToStr(sleepSec) +
            " seconds");

         sleep(sleepSec);

         LogContext(logContext).log(Log_DEBUG, "Trying to unlock '" + logInfo + "'");

         entryLockStore.unlock(lock);

         LogContext(logContext).log(Log_DEBUG, "Unlocked '" + logInfo + "'");
      }

   private:
      EntryLockStore& entryLockStore;
      const std::string dirID;
      const bool writeLock;
};

TEST(BuddyMirroring, simpleEntryLocks)
{
   EntryLockStore entryLockStore;

   const unsigned numThreadsEach = 15;

   std::list<PThread*> threadList;

   // prepare ParentNameLocks

   std::string parentIDA = "PIDA";
   std::string nameA = "NAMEA";

   std::string parentIDB = "PIDB";
   std::string nameB = "NAMEB";

   unsigned i=0;
   for ( ; i<numThreadsEach/2; i++)
   {
      ParentNameLockTestThread* t = new ParentNameLockTestThread(i, entryLockStore, parentIDA,
         nameA);
      threadList.push_back(t);
   }

   for ( ; i<numThreadsEach; i++)
   {
      ParentNameLockTestThread* t = new ParentNameLockTestThread(i, entryLockStore, parentIDB,
         nameB);
      threadList.push_back(t);
   }

   // prepare FileIDLocks

   std::string fileIDA = "fileIDA";
   std::string fileIDB = "fileIDB";

   i=0;
   for ( ; i<numThreadsEach/2; i++)
   {
      FileIDLockTestThread* t = new FileIDLockTestThread(i, entryLockStore, fileIDA);
      threadList.push_back(t);
   }

   for ( ; i<numThreadsEach; i++)
   {
      FileIDLockTestThread* t = new FileIDLockTestThread(i, entryLockStore, fileIDB);
      threadList.push_back(t);
   }

   // run all threads

   for ( std::list<PThread*>::iterator iter = threadList.begin();
      iter != threadList.end(); iter++ )
      (*iter)->start();

   for ( std::list<PThread*>::iterator iter = threadList.begin();
      iter != threadList.end(); iter++ )
      (*iter)->join();

   for ( std::list<PThread*>::iterator iter = threadList.begin();
      iter != threadList.end(); iter++ )
      SAFE_DELETE(*iter);
}

TEST(BuddyMirroring, rwEntryLocks)
{
   EntryLockStore entryLockStore;

   const unsigned numThreadsRead = 15;
   const unsigned numThreadsWrite = 5;

   std::list<PThread*> threadList;

   // prepare FileIDLocks for Directories
   std::string dirID = "dirID";
   for (unsigned i=0 ; i<numThreadsWrite; i++)
   {
      DirIDLockTestThread* t = new DirIDLockTestThread(i, entryLockStore, dirID, true);
      threadList.push_back(t);
   }

   for (unsigned i=0 ; i<numThreadsRead; i++)
   {
      DirIDLockTestThread* t = new DirIDLockTestThread(i, entryLockStore, dirID, false);
      threadList.push_back(t);
   }

   // run all threads

   for ( std::list<PThread*>::iterator iter = threadList.begin();
      iter != threadList.end(); iter++ )
      (*iter)->start();

   for ( std::list<PThread*>::iterator iter = threadList.begin();
      iter != threadList.end(); iter++ )
      (*iter)->join();

   for ( std::list<PThread*>::iterator iter = threadList.begin();
      iter != threadList.end(); iter++ )
      SAFE_DELETE(*iter);
}
