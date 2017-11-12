#include <program/Program.h>
#include "ExternalJobRunner.h"


#define JOBRUNNER_OUTPUT_DIR "/tmp/beegfs/jobrunner"


ExternalJobRunner::ExternalJobRunner() : PThread("ExternalJobRunner"),
   log("ExternalJobRunner"),
   idCounter(0)
{
   // create the temporary directory, where job outputs will be saved
   Path outputDir(JOBRUNNER_OUTPUT_DIR);
   bool createRes = StorageTk::createPathOnDisk(outputDir, false);

   if(!createRes)
   {
      throw ComponentInitException(
         "Could not create path for JobRunner output: " JOBRUNNER_OUTPUT_DIR);
   }
}

void ExternalJobRunner::run()
{
   try
   {
      log.log(Log_DEBUG, "Component started.");

      registerSignalHandler();
      workLoop();

      log.log(Log_DEBUG, "Component stopped.");
   }
   catch(std::exception& e)
   {
      Program::getApp()->handleComponentException(e);
   }
}

void ExternalJobRunner::workLoop()
{
   int jobAddWaitMS = 2500;

   while(!getSelfTerminate() )
   {
      SafeMutexLock mutexLock(&jobsMutex); // L O C K _ R U N N E R

      if(waitingJobs.empty() )
         jobAddedCond.timedwait(&jobsMutex, jobAddWaitMS);

      if(waitingJobs.empty() )
      {
         mutexLock.unlock(); // U N L O C K _ R U N N E R
         continue;
      }

      // take the oldest job from the front of the queue and execute it

      Job *job = waitingJobs.front();
      waitingJobs.pop_front();

      /* (note: we move the job to finished queue here to avoid taking the runner lock again later.
         nobody will care about it being in the finished queue as long as we don't broadcast that
         it's finished.) */
      finishedJobs.push_back(job);

      mutexLock.unlock(); // U N L O C K _ R U N N E R


      SafeMutexLock jobMutexLock(job->mutex); // L O C K _ J O B

      std::string cmd = job->cmd + " > " + job->outputFile + " 2>&1";
      job->startTime = time(NULL);

      log.log(Log_DEBUG, "Executing job: " + cmd);

      /*
      * In mongoose.c we disabled ignore SIGCHILD. Ignore SIGCHILD breaks the installation process
      * from the admon GUI, the system call system() returns every time -1. SIGCHILD is required for
      * the system call system().
      */
      int systemRes = system(cmd.c_str() );

      if(WIFEXITED(systemRes) )
         job->returnCode = WEXITSTATUS(systemRes);
      else
         job->returnCode = -1;

      if(job->returnCode != 0)
      {
         log.log(Log_ERR, "Job complete with return code: " + StringTk::intToStr(job->returnCode) +
            " - command: " + job->cmd);
      }
      else
      {
         log.log(Log_SPAM, "Job complete with return code: " + StringTk::intToStr(job->returnCode) +
            " - command: " + job->cmd);
      }

      job->finished = true;

      job->jobFinishedCond.broadcast();

      jobMutexLock.unlock(); // U N L O C K _ J O B
   }
}

/*
 * Adds a job to the jobQueue.
 * Jobs are not deleted, calling functions are responsible for deleting jobs.
 *
 * @param cmd the shell command to be executed
 * @param mutex pointer to a mutex to use for locking operations for this job
 */
Job* ExternalJobRunner::addJob(std::string cmd, Mutex* mutex)
{
   Job* job = new Job();

   job->mutex = mutex;

   job->cmd = cmd;
   job->pid = -1;
   job->returnCode = 0;
   job->startTime = 0;
   job->finished = false;


   SafeMutexLock mutexLock(&jobsMutex); // L O C K

   uint64_t nowTime = time(NULL);

   // (these depend on idCounter, which is protected by mutex)
   job->id = StringTk::uint64ToHexStr(nowTime) + "-" + StringTk::uintToHexStr(idCounter);
   job->outputFile = JOBRUNNER_OUTPUT_DIR "/" + job->id;

   idCounter++;

   waitingJobs.push_back(job);
   jobAddedCond.broadcast();

   mutexLock.unlock(); // U N L O C K


   return job;
}

/*
 * intended to delete finished jobs.
 *
 * The job is removed from the list and delete() is called.
 *
 * @param id The ID (job->id) of the job to delete.
 */
bool ExternalJobRunner::delJob(std::string id)
{
   bool retVal = false;

   SafeMutexLock safeLock(&jobsMutex); // L O C K

   for(JobListIter iter = finishedJobs.begin(); iter != finishedJobs.end(); iter++)
   {
      if (id == (*iter)->id)
      { // jobID matches
         Job* job = *iter;

         remove( ((*iter)->outputFile).c_str() );

         SAFE_DELETE(job);
         finishedJobs.erase(iter);

         retVal = true;
         break;
      }
   }

   safeLock.unlock(); // U N L O C K

   return retVal;
}

ExternalJobRunner::~ExternalJobRunner()
{
   /* make sure all memory is freed and output files are cleaned up */

   for (JobListIter iter = finishedJobs.begin(); iter != finishedJobs.end(); iter++)
   {
      remove(((*iter)->outputFile).c_str());
      delete (*iter);
   }

   for (JobListIter iter = waitingJobs.begin(); iter != waitingJobs.end(); iter++)
   {
      remove(((*iter)->outputFile).c_str());
      delete (*iter);
   }
}
