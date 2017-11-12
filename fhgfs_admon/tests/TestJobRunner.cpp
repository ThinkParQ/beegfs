#include "TestJobRunner.h"

#include <common/toolkit/StorageTk.h>
#include <components/ExternalJobRunner.h>
#include <program/Program.h>

TestJobRunner::TestJobRunner()
   : log("TestJobRunner")
{
}

TestJobRunner::~TestJobRunner()
{
}

void TestJobRunner::setUp()
{
}

void TestJobRunner::tearDown()
{
}

/*
 * use touch to create a file and test if that succeeded
 */
void TestJobRunner::testTouch()
{
   log.log(Log_DEBUG, "testTouch started");

   std::string testFile = TEST_FILE;
   int appendix = 0;

   // create the path for test file
   Path path(testFile);
   bool createPathRes = StorageTk::createPathOnDisk(path, true);

   if (!createPathRes) // we can abort right away
      CPPUNIT_FAIL("Could not setup path for test.");


   while ( StorageTk::pathExists(testFile) )
   {
      appendix++;
      testFile = TEST_FILE + StringTk::intToStr(appendix);
   }

   ExternalJobRunner* jobRunner = Program::getApp()->getJobRunner();
   Mutex mutex;
   SafeMutexLock mutexLock(&mutex);

   Job *job = jobRunner->addJob("/usr/bin/touch "+ testFile, &mutex);
   // we wait a maximum time of 5 seconds here, if a simple touch takes that long something went wrong
   job->jobFinishedCond.timedwait(&mutex, 5000);

   mutexLock.unlock();

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
         CPPUNIT_FAIL("Although job runner pretends everything went ok, file is not present.");
      }
   }
   else
   {
      remove(testFile.c_str());
      CPPUNIT_FAIL("Job Runner could not call command touch.");
   }

   /* TODO : return value of remove is ignored now;
    * maybe we should notify the user here (but that
    * would break test output)
    */
   remove(testFile.c_str());

   log.log(Log_DEBUG, "testTouch finished");
}

/*
 * try to run some non-existant command and check if return code != 0
 */
void TestJobRunner::testUnknownCmd()
{
   log.log(Log_DEBUG, "testUnknownCmd started");

   // we guess this path is not an executable
   std::string exePath = "/path/to/nonexistant/executable";

   ExternalJobRunner* jobRunner = Program::getApp()->getJobRunner();
   Mutex mutex;
   SafeMutexLock mutexLock(&mutex);

   Job *job = jobRunner->addJob(exePath, &mutex);
   job->jobFinishedCond.wait(&mutex);

   mutexLock.unlock();

   int jobRetVal = job->returnCode;
   std::string jobID = job->id;

   // delete the job
   jobRunner->delJob(jobID);

   // check result
   CPPUNIT_ASSERT(jobRetVal != 0);

   log.log(Log_DEBUG, "testUnknownCmd finished");
}
