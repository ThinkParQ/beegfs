#include "BuddyResyncer.h"

#include <program/Program.h>

__thread MetaSyncCandidateFile* BuddyResyncer::currentThreadChangeSet = 0;

BuddyResyncer::~BuddyResyncer()
{
   if (job)
   {
      job->abort(false);
      job->join();

      SAFE_DELETE(job);
   }
}

FhgfsOpsErr BuddyResyncer::startResync()
{
   std::lock_guard<Mutex> lock(jobMutex);

   if (noNewResyncs)
      return FhgfsOpsErr_INTERRUPTED;

   if (!job)
   {
      job = new BuddyResyncJob();
      job->start();
      return FhgfsOpsErr_SUCCESS;
   }

   switch (job->getState())
   {
      case BuddyResyncJobState_NOTSTARTED:
      case BuddyResyncJobState_RUNNING:
         return FhgfsOpsErr_INUSE;

      default:
         // a job must never be started more than once. to ensure this, we create a new job for
         // every resync process, because doing so allows us to use NOTSTARTED and RUNNING as
         // "job is currently active" values. otherwise, a second resync may see state SUCCESS and
         // allow duplicate resyncer activity.
         // if a job is still active, don't wait for very long - it may take a while to finish. the
         // internode syncer will retry periodically, so this will work fine.
         if (!job->timedjoin(10))
            return FhgfsOpsErr_INUSE;

         delete job;
         job = new BuddyResyncJob();
         job->start();
         return FhgfsOpsErr_SUCCESS;
   }
}

void BuddyResyncer::shutdown()
{
   std::unique_ptr<BuddyResyncJob> job;

   {
      std::lock_guard<Mutex> lock(jobMutex);

      job.reset(this->job);
      this->job = nullptr;
      noNewResyncs = true;
   }

   if (job)
   {
      job->abort(false);
      job->join();
   }
}

void BuddyResyncer::commitThreadChangeSet()
{
   BEEGFS_BUG_ON(!currentThreadChangeSet, "no change set active");

   auto* job = Program::getApp()->getBuddyResyncer()->getResyncJob();

   std::unique_ptr<MetaSyncCandidateFile> candidate(currentThreadChangeSet);
   currentThreadChangeSet = nullptr;

   Barrier syncDone(2);

   candidate->prepareSignal(syncDone);

   job->enqueue(std::move(*candidate), PThread::getCurrentThread());
   syncDone.wait();
}
