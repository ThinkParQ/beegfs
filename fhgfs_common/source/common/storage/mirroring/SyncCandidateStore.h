#ifndef SYNCCANDIDATESTORE_H
#define SYNCCANDIDATESTORE_H

#include <common/threading/Condition.h>
#include <common/threading/Mutex.h>
#include <common/threading/PThread.h>
#include <common/toolkit/ListTk.h>

#include <mutex>

template <typename SyncCandidateDir, typename SyncCandidateFile>
class SyncCandidateStore
{
   public:
       SyncCandidateStore()
          : numQueuedFiles(0), numQueuedDirs(0)
       { }

   private:
      typedef std::list<SyncCandidateFile> CandidateFileList;
      CandidateFileList candidatesFile;
      Mutex candidatesFileMutex;
      Condition filesAddedCond;
      Condition filesFetchedCond;

      typedef std::list<SyncCandidateDir> CandidateDirList;
      CandidateDirList candidatesDir;
      Mutex candidatesDirMutex;
      Condition dirsAddedCond;
      Condition dirsFetchedCond;

      // mainly used to avoid constant calling of size() method of lists
      unsigned numQueuedFiles;
      unsigned numQueuedDirs;

      static const unsigned MAX_QUEUE_SIZE = 50000;

   public:
      void add(SyncCandidateFile entry, PThread* caller)
      {
         static const unsigned waitTimeoutMS = 1000;

         std::lock_guard<Mutex> mutexLock(candidatesFileMutex);

         // wait if list is too big
         while (numQueuedFiles > MAX_QUEUE_SIZE)
         {
            if (caller && unlikely(caller->getSelfTerminate() ) )
               break; // ignore limit if selfTerminate was set to avoid hanging on shutdown

            filesFetchedCond.timedwait(&candidatesFileMutex, waitTimeoutMS);
         }

         this->candidatesFile.push_back(std::move(entry));
         numQueuedFiles++;

         filesAddedCond.signal();
      }

      void fetch(SyncCandidateFile& outCandidate, PThread* caller)
      {
         static const unsigned waitTimeMS = 3000;

         std::lock_guard<Mutex> mutexLock(candidatesFileMutex);

         while (candidatesFile.empty() )
         {
            if(caller && unlikely(caller->getSelfTerminate() ) )
            {
               outCandidate = SyncCandidateFile();
               return;
            }

            filesAddedCond.timedwait(&candidatesFileMutex, waitTimeMS);
         }

         outCandidate = std::move(candidatesFile.front());
         candidatesFile.pop_front();
         numQueuedFiles--;
         filesFetchedCond.signal();
      }

      void add(SyncCandidateDir entry, PThread* caller)
      {
         static const unsigned waitTimeoutMS = 3000;

         std::lock_guard<Mutex> mutexLock(candidatesDirMutex);

         // wait if list is too big
         while (numQueuedDirs > MAX_QUEUE_SIZE)
         {
            if (caller && unlikely(caller->getSelfTerminate() ) )
               break; // ignore limit if selfTerminate was set to avoid hanging on shutdown

            dirsFetchedCond.timedwait(&candidatesDirMutex, waitTimeoutMS);
         }

         this->candidatesDir.push_back(std::move(entry));
         numQueuedDirs++;

         dirsAddedCond.signal();
      }

      bool waitForFiles(PThread* caller)
      {
         static const unsigned waitTimeoutMS = 3000;

         std::lock_guard<Mutex> mutexLock(candidatesFileMutex);

         while (numQueuedFiles == 0)
         {
            if (caller && caller->getSelfTerminate())
               return false;

            filesAddedCond.timedwait(&candidatesFileMutex, waitTimeoutMS);
         }

         return true;
      }

      void fetch(SyncCandidateDir& outCandidate, PThread* caller)
      {
         static const unsigned waitTimeMS = 3000;

         std::lock_guard<Mutex> mutexLock(candidatesDirMutex);

         while (candidatesDir.empty() )
         {
            if(caller && unlikely(caller->getSelfTerminate() ) )
            {
               outCandidate = SyncCandidateDir();
               return;
            }

            dirsAddedCond.timedwait(&candidatesDirMutex, waitTimeMS);
         }

         outCandidate = std::move(candidatesDir.front());
         candidatesDir.pop_front();
         numQueuedDirs--;
         dirsFetchedCond.signal();
      }

      bool isFilesEmpty()
      {
         std::lock_guard<Mutex> mutexLock(candidatesFileMutex);
         return candidatesFile.empty();
      }

      bool isDirsEmpty()
      {
         std::lock_guard<Mutex> mutexLock(candidatesDirMutex);
         return candidatesDir.empty();
      }

      void waitForFiles(const unsigned timeoutMS)
      {
         std::lock_guard<Mutex> mutexLock(candidatesFileMutex);

         if (candidatesFile.empty() )
         {
            if (timeoutMS == 0)
               filesAddedCond.wait(&candidatesFileMutex);
            else
               filesAddedCond.timedwait(&candidatesFileMutex, timeoutMS);
         }
      }

      void waitForDirs(const unsigned timeoutMS)
      {
         std::lock_guard<Mutex> mutexLock(candidatesDirMutex);

         if (candidatesDir.empty() )
         {
            if (timeoutMS == 0)
               dirsAddedCond.wait(&candidatesDirMutex);
            else
               dirsAddedCond.timedwait(&candidatesDirMutex, timeoutMS);
         }
      }

      size_t getNumFiles()
      {
         std::lock_guard<Mutex> mutexLock(candidatesFileMutex);
         return candidatesFile.size();
      }

      size_t getNumDirs()
      {
         std::lock_guard<Mutex> mutexLock(candidatesDirMutex);
         return candidatesDir.size();
      }

      void clear()
      {
         {
            std::lock_guard<Mutex> dirMutexLock(candidatesDirMutex);
            candidatesDir.clear();
            numQueuedDirs = 0;
         }

         {
            std::lock_guard<Mutex> fileMutexLock(candidatesFileMutex);
            candidatesFile.clear();
            numQueuedFiles = 0;
         }
      }
};

#endif /* CHUNKSYNCCANDIDATESTORE_H */
