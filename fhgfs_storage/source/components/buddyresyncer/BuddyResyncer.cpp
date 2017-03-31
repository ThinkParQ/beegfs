#include <program/Program.h>

#include "BuddyResyncer.h"

BuddyResyncer::~BuddyResyncer()
{
   // delete remaining jobs
   for (BuddyResyncJobMapIter iter = resyncJobMap.begin(); iter != resyncJobMap.end(); iter++)
   {
      BuddyResyncJob* job = iter->second;
      if( job->isRunning() )
      {
         job->abort();
         job->join();
      }

      SAFE_DELETE(job);
   }
}

/**
 * @return FhgfsOpsErr_SUCCESS if everything was successfully started, FhgfsOpsErr_INUSE if already
 * running
 */
FhgfsOpsErr BuddyResyncer::startResync(uint16_t targetID)
{
   bool isNewJob;

   // try to add an existing resync job; if it already exists, we get that
   BuddyResyncJob* resyncJob = addResyncJob(targetID, isNewJob);

   // Job already exists *and* is already running:
   if (!isNewJob && resyncJob->isRunning() )
      return FhgfsOpsErr_INUSE;

   // job is ready and not running
   resyncJob->start();

   return FhgfsOpsErr_SUCCESS;
}
