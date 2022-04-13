#include <common/system/System.h>
#include <common/toolkit/StorageTk.h>
#include <common/toolkit/StringTk.h>
#include <components/worker/StorageBenchWork.h>
#include <program/Program.h>
#include "StorageBenchSlave.h"

#include <mutex>

#define STORAGEBENCH_STORAGE_SUBDIR_NAME              "benchmark"
#define STORAGEBENCH_READ_PIPE_TIMEOUT_MS             2000


/*
 * initialize and starts the storage benchmark with the given informations
 *
 * @param targetIDs a list with the targetIDs which the benchmark tests
 * @param blocksize the blocksize for the benchmark
 * @param size the size for the benchmark
 * @param threads the number (simulated clients) of threads for the benchmark
 * @param type the type of the benchmark
 * @return the error code, 0 if the benchmark was initialize successful (STORAGEBENCH_ERROR..)
 *
 */
int StorageBenchSlave::initAndStartStorageBench(UInt16List* targetIDs, int64_t blocksize,
   int64_t size, int threads, bool odirect, StorageBenchType type)
{
   const char* logContext = "Storage Benchmark (init)";

   int lastError = STORAGEBENCH_ERROR_NO_ERROR;
   int retVal = STORAGEBENCH_ERROR_NO_ERROR;

   this->resetSelfTerminate();

   const std::lock_guard<Mutex> lock(statusMutex);

   if (STORAGEBENCHSTATUS_IS_ACTIVE(this->status))
   {
      LogContext(logContext).logErr(
         std::string("Benchmark is already running. It's not possible to start a benchmark if a"
            "benchmark is running."));

      retVal = STORAGEBENCH_ERROR_RUNTIME_IS_RUNNING;
   }
   else
   {
      retVal = initStorageBench(targetIDs, blocksize, size, threads, odirect, type);
   }

   if(retVal == STORAGEBENCH_ERROR_NO_ERROR)
   {
      if (this->status != StorageBenchStatus_INITIALISED)
      {
         LogContext(logContext).logErr(
            std::string("Benchmark not correctly initialized."));
         this->lastRunErrorCode = STORAGEBENCH_ERROR_UNINITIALIZED;
         this->status = StorageBenchStatus_ERROR;

         retVal = STORAGEBENCH_ERROR_UNINITIALIZED;
      }
      else
      {
         try
         {
            this->start();
            this->status = StorageBenchStatus_RUNNING;
            lastError = this->lastRunErrorCode;
         }
         catch(PThreadCreateException& e)
         {
            LogContext(logContext).logErr(std::string("Unable to start thread: ") + e.what() );
            this->status = StorageBenchStatus_ERROR;
            lastError = this->lastRunErrorCode;
         }
      }
   }

   if(lastError != STORAGEBENCH_ERROR_NO_ERROR)
   {
      retVal = lastError;
   }

   return retVal;
}

/*
 * initialize the storage benchmark with the given informations
 *
 * @param targetIDs a list with the targetIDs which the benchmark tests
 * @param blocksize the blocksize for the benchmark
 * @param size the size for the benchmark
 * @param threads the number (simulated clients) of threads for the benchmark
 * @param type the type of the benchmark
 * @return the error code, 0 if the benchmark was initialize successful (STORAGEBENCH_ERROR..)
 *
 */
int StorageBenchSlave::initStorageBench(UInt16List* targetIDs, int64_t blocksize,
   int64_t size, int threads, bool odirect, StorageBenchType type)
{
   const char* logContext = "Storage Benchmark (init)";
   LogContext(logContext).log(Log_DEBUG, "Initializing benchmark ...");

   this->benchType = type;
   this->targetIDs = new auto(*targetIDs);
   this->blocksize = blocksize;
   this->size = size;
   this->numThreads = threads;
   this->odirect = odirect;
   this->numThreadsDone = 0;

   initThreadData();

   if (!initTransferData())
   {
      this->lastRunErrorCode = STORAGEBENCH_ERROR_INIT_TRANSFER_DATA;
      this->status = StorageBenchStatus_ERROR;
      return STORAGEBENCH_ERROR_INIT_TRANSFER_DATA;
   }

   if (this->benchType == StorageBenchType_READ)
   {
      if (!checkReadData())
      {
         LogContext(logContext).logErr(
            std::string("No (or not enough) data for read benchmark available. "
               "Start a write benchmark with the same size parameter before the read benchmark.") );

         this->lastRunErrorCode = STORAGEBENCH_ERROR_INIT_READ_DATA;
         this->status = StorageBenchStatus_ERROR;
         return STORAGEBENCH_ERROR_INIT_READ_DATA;
      }
   }
   else
   if (this->benchType == StorageBenchType_WRITE)
   {
      if (!createBenchmarkFolder() )
      {
         LogContext(logContext).logErr(
            std::string("Couldn't create the benchmark folder."));

         this->lastRunErrorCode = STORAGEBENCH_ERROR_INIT_CREATE_BENCH_FOLDER;
         this->status = StorageBenchStatus_ERROR;
         return STORAGEBENCH_ERROR_INIT_CREATE_BENCH_FOLDER;
      }
   }
   else
   {
      LogContext(logContext).logErr(std::string(
         "Unknown benchmark type: " + StringTk::uintToStr(this->benchType) ) );
      return STORAGEBENCH_ERROR_INITIALIZATION_ERROR;
   }

   this->lastRunErrorCode = STORAGEBENCH_ERROR_NO_ERROR;
   this->status = StorageBenchStatus_INITIALISED;

   LogContext(logContext).log(Log_DEBUG, std::string("Benchmark initialized."));

   return STORAGEBENCH_ERROR_NO_ERROR;
}

/*
 * initialize the data which will be written to the disk, the size of the transfer data a equal
 * to the blocksize and initialized with random characters
 *
 * @return true if the random data are initialized,
 *         false if a error occurred
 *
 */
bool StorageBenchSlave::initTransferData()
{
   const char* logContext = "Storage Benchmark (init buf)";
   LogContext(logContext).log(Log_DEBUG, std::string("Initializing random data..."));

   void* rawTransferData;
   if (posix_memalign(&rawTransferData, 4096, blocksize) != 0)
      return false;
   transferData.reset(static_cast<char*>(rawTransferData));

   Random randomizer = Random();

   for (int64_t counter = 0; counter < this->blocksize; counter++)
   {
      this->transferData[counter] = randomizer.getNextInt();
   }

   LogContext(logContext).log(Log_DEBUG, std::string("Random data initialized."));

   return true;
}

/*
 * frees the transfer data
 */
void StorageBenchSlave::freeTransferData()
{
   transferData.reset();
}

/*
 * initialize the informations about the threads
 *
 */
void StorageBenchSlave::initThreadData()
{
   const char* logContext = "Storage Benchmark (init)";
   LogContext(logContext).log(Log_DEBUG, std::string("Initializing thread data..."));

   this->threadData.clear();

   int allThreadCounter = 0;
   for (UInt16ListIter iter = targetIDs->begin(); iter != targetIDs->end(); iter++)
   {
      for (int threadCount = 0; threadCount < this->numThreads; threadCount++)
      {
         StorageBenchThreadData data;
         data.targetID = *iter;
         data.targetThreadID = threadCount;
         data.engagedSize = 0;
         data.fileDescriptor = 0;
         data.neededTime = 0;


         this->threadData[allThreadCounter] = data;
         allThreadCounter++;
      }
   }

   LogContext(logContext).log(Log_DEBUG, "Thread data initialized.");
}

/*
 * starts the benchmark, a read or a write benchmark
 *
 */
void StorageBenchSlave::run()
{
   const char* logContext = "Storage Benchmark (run)";
   LogContext(logContext).log(Log_CRITICAL, std::string("Benchmark started..."));

   App* app = Program::getApp();

   bool openRes = openFiles();
   if (openRes)
   {
      this->startTime.setToNow();

      // add a work package into the worker queue for every thread
      for(StorageBenchThreadDataMapIter iter = threadData.begin();
          iter != threadData.end();
          iter++)
      {
         LOG_DEBUG(logContext, Log_DEBUG, std::string("Add work for target: ") +
            StringTk::uintToStr(iter->second.targetID) );
         LOG_DEBUG(logContext, Log_DEBUG, std::string("- threadID: ") +
            StringTk::intToStr(iter->first) );
         LOG_DEBUG(logContext, Log_DEBUG, std::string("- type: ") +
            StringTk::intToStr(this->benchType) );

         StorageBenchWork* work = new StorageBenchWork(iter->second.targetID, iter->first,
            iter->second.fileDescriptor, this->benchType, getNextPackageSize(iter->first),
            this->threadCommunication, this->transferData.get());

         app->getWorkQueue(iter->second.targetID)->addIndirectWork(work);
      }

      while(getStatus() == StorageBenchStatus_RUNNING)
      {
         int threadID = 0;

         if (this->threadCommunication->waitForIncomingData(STORAGEBENCH_READ_PIPE_TIMEOUT_MS))
         {
            this->threadCommunication->getReadFD()->readExact(&threadID, sizeof(int));
         }
         else
         {
            threadID = STORAGEBENCH_ERROR_COM_TIMEOUT;
         }

         if (this->getSelfTerminate())
         {
            LogContext(logContext).logErr(std::string("Abort benchmark."));
            this->lastRunErrorCode = STORAGEBENCH_ERROR_ABORT_BENCHMARK;
            setStatus(StorageBenchStatus_STOPPING);

            if (threadID != STORAGEBENCH_ERROR_COM_TIMEOUT)
            {
               this->threadData[threadID].neededTime = this->startTime.elapsedMS();
               this->numThreadsDone++;
            }

            break;
         }
         else
         if (threadID == STORAGEBENCH_ERROR_WORKER_ERROR)
         {
            LogContext(logContext).logErr(std::string("I/O operation on disk failed."));
            this->lastRunErrorCode = STORAGEBENCH_ERROR_WORKER_ERROR;
            setStatus(StorageBenchStatus_STOPPING);

            // increment the thread counter, because the thread which sent this error hasn't a
            // work package in the queue of the workers but the response from the other threads
            // must be collected
            this->numThreadsDone++;

            break;
         }
         else
         if (threadID == STORAGEBENCH_ERROR_COM_TIMEOUT)
         {
            continue;
         }
         else
         if ( (threadID < -1) || ( ( (unsigned)threadID) >= this->threadData.size() ) )
         { // error if the worker reports an unknown threadID
            std::string errorMessage("Unknown thread ID: " + StringTk::intToStr(threadID) + "; "
               "map size: " + StringTk::uintToStr(this->threadData.size() ) );

            LogContext(logContext).logErr(errorMessage);
            this->lastRunErrorCode = STORAGEBENCH_ERROR_RUNTIME_ERROR;
            setStatus(StorageBenchStatus_STOPPING);

            // increment the thread counter, because the thread which sent this error hasn't a
            // work package in the queue of the workers but the response from the other threads
            // must be collected
            this->numThreadsDone++;

            break;
         }

         StorageBenchThreadData* currentData = &this->threadData[threadID];
         int64_t workSize = getNextPackageSize(threadID);

         // add a new work package into the workers queue for the reported thread only if the
         // data size for the thread is bigger then 0
         if (workSize != 0)
         {
            StorageBenchWork* work = new StorageBenchWork(currentData->targetID, threadID,
               currentData->fileDescriptor, this->benchType, workSize, this->threadCommunication,
               this->transferData.get());
            app->getWorkQueue(currentData->targetID)->addIndirectWork(work);
         }
         else
         {
            // the thread has finished his work
            currentData->neededTime = this->startTime.elapsedMS();
            this->numThreadsDone++;
         }

         if (this->numThreadsDone >= this->threadData.size())
         {
            setStatus(StorageBenchStatus_FINISHING);
         }
      }

      //collect all responses from the worker
      while ( (this->numThreadsDone < this->threadData.size()) && app->getWorkersRunning() )
      {
         int threadID = 0;

         if (this->threadCommunication->waitForIncomingData(STORAGEBENCH_READ_PIPE_TIMEOUT_MS))
         {
            this->threadCommunication->getReadFD()->readExact(&threadID, sizeof(int));
         }
         else
         {
            continue;
         }

         LOG_DEBUG(logContext, Log_DEBUG, std::string("Collect response from worker."));

         if(threadID >= 0)
            this->threadData[threadID].neededTime = this->startTime.elapsedMS();

         this->numThreadsDone++;
      }

      // all workers finished/stopped ==> close all files
      closeFiles();
      freeTransferData();

      // all threads have finished the work or the benchmark was stopped, set new status
      if (this->getStatus() == StorageBenchStatus_FINISHING)
      {
         this->setStatus(StorageBenchStatus_FINISHED);
         LogContext(logContext).log(Log_CRITICAL, std::string("Benchmark finished."));
      }
      else
      if (this->getStatus() == StorageBenchStatus_STOPPING)
      {
         if (this->lastRunErrorCode != STORAGEBENCH_ERROR_NO_ERROR)
         {
            this->setStatus(StorageBenchStatus_ERROR);
            LogContext(logContext).log(Log_CRITICAL, std::string("Benchmark stopped with errors."));
         }
         else
         {
            this->setStatus(StorageBenchStatus_STOPPED);
            LogContext(logContext).log(Log_CRITICAL, std::string("Benchmark stopped."));
         }
      }

   }
   else
   {
      this->lastRunErrorCode = STORAGEBENCH_ERROR_RUNTIME_OPEN_FILES;
      setStatus(StorageBenchStatus_ERROR);
   }
}

/*
 * checks the size of the benchmark files, the benchmark files must be big enough for the
 * read benchmark
 *
 * @return true if data for a read benchmark exists,
 *         false if the files to small or a error occurred
 *
 */
bool StorageBenchSlave::checkReadData()
{
   const char* logContext = "Storage Benchmark (check)";

   for (StorageBenchThreadDataMapIter iter = threadData.begin();
            iter != threadData.end(); iter++)
   {
      auto* const target = Program::getApp()->getStorageTargets()->getTarget(iter->second.targetID);
      if (!target)
      {
         LogContext(logContext).logErr(std::string("TargetID unknown."));
         return false;
      }

      std::string path = target->getPath().str();

      path = path + "/" + STORAGEBENCH_STORAGE_SUBDIR_NAME + "/" +
         StringTk::uintToStr(iter->second.targetThreadID);

      int error = -1;
      struct stat fileStat;
      error = stat(path.c_str(), &fileStat);

      if (error != -1)
      {
         if (fileStat.st_size < this->size)
         {
            LogContext(logContext).logErr(std::string("Existing benchmark file too small. "
               "Requested file size: " + StringTk::int64ToStr(this->size) + " "
               "File size: " + StringTk::intToStr(fileStat.st_size)));
            return false;
         }
      }
      else
      {
         LogContext(logContext).logErr(std::string("Couldn't stat() benchmark file. SysErr: ") +
               System::getErrString() );
         return false;
      }
   }

   return true;
}

/*
 * creates the benchmark folder in the storage target folder
 *
 * @return true if all benchmark folders are created,
 *         false if a error occurred
 *
 */
bool StorageBenchSlave::createBenchmarkFolder()
{
   const char* logContext = "Storage Benchmark (mkdir)";

   for(UInt16ListIter iter = this->targetIDs->begin(); iter != this->targetIDs->end(); iter++)
   {
      auto* const target = Program::getApp()->getStorageTargets()->getTarget(*iter);
      if (!target)
      {
         LogContext(logContext).logErr("TargetID unknown: " + StringTk::uintToStr(*iter) );
         return false;
      }

      Path currentPath(target->getPath() / STORAGEBENCH_STORAGE_SUBDIR_NAME);
      if(!StorageTk::createPathOnDisk(currentPath, false))
      {
         LogContext(logContext).logErr(
            std::string("Unable to create benchmark directory: " + currentPath.str() ) );
         return false;
      }
   }

   return true;
}

/*
 * opens all needed files for the benchmark. This method will be executed at the start
 * of the benchmark
 *
 * @return true if all files are opened,
 *         false if a error occurred
 *
 */
bool StorageBenchSlave::openFiles()
{
   const char* logContext = "Storage Benchmark (open)";
   mode_t openMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

   for(StorageBenchThreadDataMapIter iter = threadData.begin();
      iter != threadData.end();
      iter++)
   {
      auto* const target = Program::getApp()->getStorageTargets()->getTarget(iter->second.targetID);
      if (!target)
      {
         LogContext(logContext).logErr(
            "TargetID unknown: " + StringTk::uintToStr(iter->second.targetID) );
         return false;
      }

      std::string path = target->getPath().str();

      path = path + "/" STORAGEBENCH_STORAGE_SUBDIR_NAME "/" +
         StringTk::uintToStr(iter->second.targetThreadID);

      int fileDescriptor = -1;

      // open file

      int directFlag = this->odirect ? O_DIRECT : 0;
      if(this->benchType == StorageBenchType_READ)
         fileDescriptor = open(path.c_str(), O_RDONLY | directFlag);
      else
         fileDescriptor = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC | directFlag, openMode);

      if (fileDescriptor != -1)
         iter->second.fileDescriptor = fileDescriptor;
      else
      { // open failed
         LogContext(logContext).logErr("Couldn't open benchmark file: " + path + "; "
            "SysErr: " + System::getErrString() );
         return false;
      }
   }

   return true;
}

bool StorageBenchSlave::closeFiles()
{
   const char* logContext = "Storage Benchmark (close)";

   bool retVal = true;

   for(StorageBenchThreadDataMapIter iter = threadData.begin();
      iter != threadData.end();
      iter++)
   {
      int tmpRetVal = close(iter->second.fileDescriptor);

      if (tmpRetVal != 0)
      {
         int closeErrno = errno;

         auto* const target = Program::getApp()->getStorageTargets()->getTarget(
               iter->second.targetID);
         if (!target)
         {
            LogContext(logContext).logErr(
               "TargetID unknown: " + StringTk::uintToStr(iter->second.targetID) );
            return false;
         }

         std::string path = target->getPath().str();

         path = path + "/" + STORAGEBENCH_STORAGE_SUBDIR_NAME + "/" +
               StringTk::uintToStr(iter->second.targetThreadID);

         LogContext(logContext).logErr("Couldn't close file: " + path + "; "
            "SysErr: " + System::getErrString(closeErrno) );

         retVal = false;
      }
   }

   return retVal;
}

/*
 * calculates the size (bytes) of the data which will be written on the disk by the worker with
 * the next work package for the given thread
 *
 * @param threadID the threadID
 * @return the size of the data for next work package in bytes,
 *         if 0 the given thread has written all data
 *
 */
int64_t StorageBenchSlave::getNextPackageSize(int threadID)
{
   int64_t retVal = BEEGFS_MIN(this->blocksize,
      this->size - this->threadData[threadID].engagedSize);
   this->threadData[threadID].engagedSize += retVal;

   return retVal;
}


/*
 * calculates the throughput (kB/s) of the given target
 *
 * @param targetID the targetID
 * @return the throughput of the given target in kilobytes per second
 *
 */
int64_t StorageBenchSlave::getResult(uint16_t targetID)
{
   int64_t size = 0;
   int64_t time = 0;

   for(StorageBenchThreadDataMapIter iter = this->threadData.begin();
      iter != this->threadData.end();
      iter++)
   {
      if (iter->second.targetID == targetID)
      {
         // summarize the size of the different threads which worked on a target
         size += iter->second.engagedSize;

         // search the thread with the longest runtime
         if (time < this->threadData[iter->first].neededTime)
            time = this->threadData[iter->first].neededTime;

      }
   }

   // if the threads are not finished use the needed time up to now
   if (time == 0)
      time = this->startTime.elapsedMS();

   // if no results available return zero
   if ( (size == 0) || (time == 0) )
      return 0;

   // input: size in bytes, time in milliseconds,
   // output: in kilobytes per second
   return ( (size * 1000) / (time * 1024) );
}

/*
 * calculates the throughput (kB/s) of the given targets
 *
 * @param targetIDs the list of targetIDs
 * @param outResults a initialized map for the results, which contains the results after
 *        execution of the method
 *
 */
void StorageBenchSlave::getResults(UInt16List* targetIDs, StorageBenchResultsMap* outResults)
{
   for (UInt16ListIter iter = targetIDs->begin(); iter != targetIDs->end(); iter++)
   {
      (*outResults)[*iter] = getResult(*iter);
   }
}

/*
 * calculates the throughput (kB/s) of all targets
 *
 * @param outResults a initialized map for the results, which contains the results after
 *        execution of the method
 *
 */
void StorageBenchSlave::getAllResults(StorageBenchResultsMap* outResults)
{
   for (UInt16ListIter iter = this->targetIDs->begin(); iter != this->targetIDs->end(); iter++)
   {
      (*outResults)[*iter] = getResult(*iter);
   }
}

/*
 * calculates the throughput (kB/s) of the given targets and returns the status of the benchmark
 *
 * @param targetIDs the list of targetIDs
 * @param outResults a initialized map for the results, which contains the results after
 *        execution of the method
 * @return the status of the benchmark
 *
 */
StorageBenchStatus StorageBenchSlave::getStatusWithResults(UInt16List* targetIDs,
   StorageBenchResultsMap* outResults)
{
   getResults(targetIDs, outResults);
   return getStatus();
}

/*
 * stop the benchmark
 *
 * @return the error code, 0 if the benchmark will stop (STORAGEBENCH_ERROR..)
 *
 */
int StorageBenchSlave::stopBenchmark()
{
   const std::lock_guard<Mutex> lock(statusMutex);

   if (this->status == StorageBenchStatus_RUNNING)
   {
      this->status = StorageBenchStatus_STOPPING;
      return STORAGEBENCH_ERROR_NO_ERROR;
   }
   else
   if(this->status == StorageBenchStatus_FINISHING  || this->status == StorageBenchStatus_STOPPING)
   {
      return STORAGEBENCH_ERROR_NO_ERROR;
   }

   return STORAGEBENCH_ERROR_NO_ERROR;
}

/*
 * deletes all files in the benchmark folder of the given targets
 *
 * @param targetIDs the list of targetIDs which will be cleaned
 * @return the error code, 0 if the cleanup was successful (STORAGEBENCH_ERROR..)
 *
 */
int StorageBenchSlave::cleanup(UInt16List* targetIDs)
{
   const std::lock_guard<Mutex> lock(statusMutex);
   const char* logContext = "Storage Benchmark (cleanup)";

   //cleanup only possible if no benchmark is running
   if (STORAGEBENCHSTATUS_IS_ACTIVE(this->status))
   {
      LogContext(logContext).logErr("Cleanup not possible benchmark is running");

      return STORAGEBENCH_ERROR_RUNTIME_CLEANUP_JOB_ACTIVE;
   }

   for(UInt16ListIter iter = targetIDs->begin(); iter != targetIDs->end(); iter++)
   {
      auto* const target = Program::getApp()->getStorageTargets()->getTarget(*iter);
      if (!target)
      {
         LogContext(logContext).logErr(std::string("TargetID unknown."));
         return STORAGEBENCH_ERROR_RUNTIME_UNKNOWN_TARGET;
      }

      std::string path = target->getPath().str();

      path.append("/");
      path.append(STORAGEBENCH_STORAGE_SUBDIR_NAME);
      path.append("/");

      DIR* dir = opendir(path.c_str());
      if (dir == NULL)
      {
         int openDirErrno = errno;
         int errRetVal;

         if (openDirErrno == ENOENT)
         { // benchmark directory doesn't exist, no benchmark data for cleanup
            errRetVal = STORAGEBENCH_ERROR_NO_ERROR;
         }
         else
         {
            this->lastRunErrorCode = STORAGEBENCH_ERROR_RUNTIME_DELETE_FOLDER;
            errRetVal = STORAGEBENCH_ERROR_RUNTIME_DELETE_FOLDER;

            LogContext(logContext).logErr("Unable to delete files in benchmark directory: " + path +
               "; failed with SysErr: " + System::getErrString(errno));
         }

         return errRetVal;
      }

      struct dirent* dirEntry = StorageTk::readdirFiltered(dir);

      while (dirEntry)
      {
         struct stat statData;
         std::string filePath(path + dirEntry->d_name);

         int retVal = stat(filePath.c_str(), &statData);
         if ((retVal == 0) && (S_ISREG(statData.st_mode)) )
         {

            int error = unlink(filePath.c_str());

            if(error != 0)
            {
               LogContext(logContext).logErr(
                  std::string("Unable to delete files in benchmark directory: "
                  + path));
               this->lastRunErrorCode = STORAGEBENCH_ERROR_RUNTIME_DELETE_FOLDER;

               closedir(dir);
               return STORAGEBENCH_ERROR_RUNTIME_DELETE_FOLDER;
            }
         }
         else
         if(!S_ISREG(statData.st_mode))
             LogContext(logContext).logErr("Unable to delete files in benchmark directory: " +
                path + " It's not a regular file.");
         else
            LogContext(logContext).logErr("Unable to delete files in benchmark directory: " + path);

         dirEntry = StorageTk::readdirFiltered(dir);
      }

      closedir(dir);
   }

   return STORAGEBENCH_ERROR_NO_ERROR;
}

/*
 * aborts the benchmark, will be used if SIGINT received
 *
 */
void StorageBenchSlave::shutdownBenchmark()
{
   this->selfTerminate();
}

void StorageBenchSlave::waitForShutdownBenchmark()
{
   const std::lock_guard<Mutex> lock(statusMutex);

   while(STORAGEBENCHSTATUS_IS_ACTIVE(this->status))
   {
      this->statusChangeCond.wait(&this->statusMutex);
   }
}
