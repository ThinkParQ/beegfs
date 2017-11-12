#ifndef BUDDYRESYNCERGATHERSLAVE_H_
#define BUDDYRESYNCERGATHERSLAVE_H_

#include <common/app/log/LogContext.h>
#include <common/threading/PThread.h>
#include <components/buddyresyncer/SyncCandidate.h>

#include <mutex>

class BuddyResyncerGatherSlave : public PThread
{
   // Grant access to internal mutex
   friend class BuddyResyncer;
   friend class BuddyResyncJob;

   public:
      BuddyResyncerGatherSlave(MetaSyncCandidateStore* syncCandidates);

      void workLoop();

   private:
      Mutex stateMutex;
      Condition isRunningChangeCond;

      AtomicUInt64 numDirsDiscovered;
      AtomicUInt64 numErrors;

      std::string metaBuddyPath;

      bool isRunning;

      MetaSyncCandidateStore* syncCandidates;

      virtual void run();

      void crawlDir(const std::string& path, const MetaSyncDirType type, const unsigned level = 0);

   public:
      bool getIsRunning()
      {
         std::lock_guard<Mutex> lock(stateMutex);
         return this->isRunning;
      }

      struct Stats
      {
         uint64_t dirsDiscovered;
         uint64_t errors;
      };

      Stats getStats()
      {
         return Stats{ numDirsDiscovered.read(), numErrors.read() };
      }


   private:
      void setIsRunning(const bool isRunning)
      {
         std::lock_guard<Mutex> lock(stateMutex);
         this->isRunning = isRunning;
         isRunningChangeCond.broadcast();
      }

      void addCandidate(const std::string& path, const MetaSyncDirType type)
      {
         const std::string& relPath = path.substr(metaBuddyPath.size() + 1);
         syncCandidates->add(MetaSyncCandidateDir(relPath, type), this);
      }
};

typedef std::vector<BuddyResyncerGatherSlave*> BuddyResyncerGatherSlaveVec;
typedef BuddyResyncerGatherSlaveVec::iterator BuddyResyncerGatherSlaveVecIter;

#endif /* BUDDYRESYNCERGATHERSLAVE_H_ */
