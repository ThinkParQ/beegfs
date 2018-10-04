#include <common/toolkit/StorageTk.h>
#include <components/ExternalJobRunner.h>
#include <program/Program.h>

#include <mutex>

#include <gtest/gtest.h>

#define TEST_FILE "/tmp/beegfs_admon/testing/jobrunner/test.db"

/*
 * use touch to create a file and test if that succeeded
 */
TEST(JobRunner, DISABLED_touch)
{
   std::string testFile = TEST_FILE;
   int appendix = 0;

   // create the path for test file
   Path path(testFile);
   bool createPathRes = StorageTk::createPathOnDisk(path, true);

   if (!createPathRes) // we can abort right away
      FAIL() << "Could not setup path for test.";


   while ( StorageTk::pathExists(testFile) )
   {
      appendix++;
      testFile = TEST_FILE + StringTk::intToStr(appendix);
   }

   ExternalJobRunner* jobRunner = Program::getApp()->getJobRunner();
   Job* job = nullptr;
   Mutex mutex;
   {
      const std::lock_guard<Mutex> lock(mutex);

      job = jobRunner->addJob("/usr/bin/touch "+ testFile, &mutex);
      // we wait a maximum time of 5 seconds here, if a simple touch takes that long something went wrong
      job->jobFinishedCond.timedwait(&mutex, 5000);
   }

   int jobRetVal = job->returnCode;
   std::string jobID = job->id;

   // delete the job
   jobRunner->delJob(jobID);

   // check result
   if ( jobRetVal == 0 )
   {
      // job says everything's ok...confirm that
      if ( !StorageTk::pathExists(testFile) )
      {
         remove(testFile.c_str());
         FAIL() << "Although job runner pretends everything went ok, file is not present.";
      }
   }
   else
   {
      remove(testFile.c_str());
      FAIL() << "Job Runner could not call command touch.";
   }

   /* TODO : return value of remove is ignored now;
    * maybe we should notify the user here (but that
    * would break test output)
    */
   remove(testFile.c_str());
}

/*
 * try to run some non-existant command and check if return code != 0
 */
TEST(JobRunner, DISABLED_unknownCmd)
{
   // we guess this path is not an executable
   std::string exePath = "/path/to/nonexistant/executable";

   ExternalJobRunner* jobRunner = Program::getApp()->getJobRunner();
   Mutex mutex;
   Job* job = nullptr;
   {
      const std::lock_guard<Mutex> lock(mutex);

      job = jobRunner->addJob(exePath, &mutex);
      job->jobFinishedCond.wait(&mutex);
   }

   int jobRetVal = job->returnCode;
   std::string jobID = job->id;

   // delete the job
   jobRunner->delJob(jobID);

   // check result
   ASSERT_NE(jobRetVal, 0);
}
