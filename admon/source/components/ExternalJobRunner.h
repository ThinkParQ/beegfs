/*
 * ExternalJobRunner is intended to be used to run some external commands in the system (so
 * basically it can be seen as wrapper around the 'system' command).
 * It gives the possibility to run a command, determine if it is finsihed runnning and gives you
 * access to the return code and output
 */

#ifndef EXTERNALJOBRUNNER_H_
#define EXTERNALJOBRUNNER_H_

#include <common/app/log/LogContext.h>
#include <common/threading/PThread.h>
#include <common/toolkit/StringTk.h>
#include <common/Common.h>
#include <common/threading/Condition.h>


struct Job
{
      std::string id;
      uint64_t startTime; // set when job execution begins
      int pid; // currently unused
      std::string cmd;
      int returnCode;
      std::string outputFile;
      bool finished;
      Mutex *mutex; // set on addJob()
      Condition jobFinishedCond;
};


typedef std::list<Job*> JobList;
typedef std::list<Job*>::iterator JobListIter;


class ExternalJobRunner : public PThread
{
   private:
      LogContext log;

      // all jobs are listed here and are processed one after another
      JobList waitingJobs; // new jobs pushed to back
      JobList finishedJobs;
      uint idCounter;

      Mutex jobsMutex; // protects job lists and idCounter
      Condition jobAddedCond;

      void workLoop();

   public:
      ExternalJobRunner();
      virtual ~ExternalJobRunner();

      Job* addJob(std::string cmd, Mutex* mutex);
      bool delJob(std::string id);

      virtual void run();
};

#endif /* EXTERNALJOBRUNNER_H_ */
