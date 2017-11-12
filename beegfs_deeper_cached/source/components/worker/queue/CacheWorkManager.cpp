#include <deeper/deeper_cache.h>
#include <program/Program.h>
#include "CacheWorkManager.h"



/**
 * Constructor.
 */
CacheWorkManager::CacheWorkManager()
{
   numPendingWorks = 0;

   fifo = new std::queue<DeeperWork*>();
   directWorkList = new ListWorkContainer();

   worksWaitToProcess = new CacheWorkHashMap();
   worksInProcessing = new CacheWorkHashMap();
   worksFailed = new CacheWorkFailedHashMap();
   worksCrcChecksums = new CacheWorkCrcSumHashMap();
}

/**
 * Destructor.
 */
CacheWorkManager::~CacheWorkManager()
{
#ifdef BEEGFS_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, std::string("Size of lists: ") +
      "fifo: " + StringTk::uint64ToStr(fifo->size() ) +
      "; directWorkList: " +
      StringTk::uint64ToStr( ( (ListWorkContainer*)directWorkList)->getSize() ) +
      "; worksWaitToProcess: " + StringTk::uint64ToStr(worksWaitToProcess->size() ) +
      "; worksInProcessing: " + StringTk::uint64ToStr(worksInProcessing->size() ) +
      "; worksFailed: " + StringTk::uint64ToStr(worksFailed->size() ) +
      "; worksCrcChecksums: " + StringTk::uint64ToStr(worksCrcChecksums->size() ) );
#endif

   worksWaitToProcess->clear();
   worksInProcessing->clear();
   worksFailed->clear();
   worksCrcChecksums->clear();

   SAFE_DELETE(fifo);
   SAFE_DELETE(directWorkList);

   SAFE_DELETE(worksWaitToProcess);
   SAFE_DELETE(worksInProcessing);
   SAFE_DELETE(worksFailed);
   SAFE_DELETE(worksCrcChecksums);
}

/**
 * Add a work package to the fifo and to the lookup list for work to process.
 *
 * @param work The work package to process.
 * @param allowAsSplit Allows the work to be added as a split work to an existing work.
 * @return DEEPER_RETVAL_SUCCESS if work added successful, else the value exists in the map it
 *         returns EALREADY.
 */
int CacheWorkManager::addWork(Work* work, bool allowAsSplit)
{
   int retVal = EALREADY;

   DeeperWork* cacheWork = (DeeperWork*) work;
   CacheWorkKey key;
   cacheWork->getKey(key);

   SafeRWLock rwLock(&rwlockMaps, SafeRWLock_WRITE);           // W R I T E L O C K

   std::pair<CacheWorkHashMapIter, bool> insertIter;

   // check if a corresponding work is in processing
   CacheWorkHashMapConstIter findIter = worksInProcessing->find(key);
   if(findIter != worksInProcessing->end() )
   {
      if(!allowAsSplit)
      {
         errno = EALREADY;
         Logger::getLogger()->log(Log_ERR, __FUNCTION__, "Could not add work: " +
            cacheWork->printForLog() + "; errno: " + System::getErrString(errno) );
      }
      else
      {
         ((DeeperWork*)findIter->second)->addSplitWork();
         cacheWork->changeToSplitWork(findIter->second);
         fifo->push(cacheWork);

         retVal = DEEPER_RETVAL_SUCCESS;
      }
      goto unlock;
   }

   insertIter = worksWaitToProcess->insert(CacheWorkHashMapVal(key, cacheWork) );
   if(insertIter.second)
   {
      fifo->push(cacheWork);
      worksFailed->erase(key);         // remove old failed work, ignore return value
      worksCrcChecksums->erase(key);   // remove old CRC checksum work, ignore return value

      retVal = DEEPER_RETVAL_SUCCESS;
      goto unlock;
   }
   else
   {
      if(!allowAsSplit)
      {
         errno = EALREADY;
         Logger::getLogger()->log(Log_ERR, __FUNCTION__, "Could not add work: " +
            cacheWork->printForLog() + "; errno: " + System::getErrString(errno) );
      }
      else
      {
         ((DeeperWork*)insertIter.first->second)->addSplitWork();
         cacheWork->changeToSplitWork(insertIter.first->second);
         fifo->push(cacheWork);

         retVal = DEEPER_RETVAL_SUCCESS;
      }
      goto unlock;
   }

unlock:
   rwLock.unlock();                                            // U N L O C K - R/W

   if(retVal == DEEPER_RETVAL_SUCCESS)
   {
      SafeMutexLock mutexLock(&newWorkMutex);                  // L O C K
      numPendingWorks++;
      newWorkCond.signal();
      mutexLock.unlock();                                      // U N L O C K
   }

   return retVal;
}

/**
 * Adds a split of a work only to the fifo, because the parent of the split is part of the maps.
 * A split of a work is a rang flush or ranged prefetch or a discard.
 *
 * @param work The work package to process.
 * @return DEEPER_RETVAL_SUCCESS if work added successful, else the value exists in the map it
 *         returns EEXIST.
 */
int CacheWorkManager::addSplitWork(Work* work)
{
   DeeperWork* cacheWork = (DeeperWork*) work;

   SafeRWLock rwLock(&rwlockMaps, SafeRWLock_WRITE);           // W R I T E L O C K

   fifo->push(cacheWork);

   rwLock.unlock();                                         // U N L O C K - R/W


   SafeMutexLock mutexLock(&newWorkMutex);                  // L O C K

   numPendingWorks++;
   newWorkCond.signal();

   mutexLock.unlock();                                      // U N L O C K

   return DEEPER_RETVAL_SUCCESS;
}

/**
 * Add a work package to the fifo and to the lookup list for work to process.
 *
 * Note: It is just a wrapper for the preferred addWork(Work*) function which allows error handling.
 *
 * @param work The work package to process.
 * @param userID The UID of the user process.
 */
void CacheWorkManager::addWork(Work* work, unsigned userID)
{
   addWork(work, false);
}

/**
 * Fetches the next work from the fifo of waiting work packages and adds a reference to the work in
 * processing map.
 *
 * @return The next work to process.
 */
CacheWork* CacheWorkManager::getAndPopNextWork()
{
   CacheWork* nextWork = NULL;

   SafeRWLock rwLock(&rwlockMaps, SafeRWLock_WRITE);              // W R I T E L O C K

   nextWork = fifo->front();
   fifo->pop();

   if(!nextWork->isSplitWork() )
   { // must only be done for the parent of a split work
      CacheWorkKey key;
      nextWork->getKey(key);
      worksInProcessing->insert(CacheWorkHashMapVal(key, nextWork) );
      worksWaitToProcess->erase(key);
   }

   rwLock.unlock();                                               // U N L O C K - R/W

   return nextWork;
}

/**
 * Checks if the work is listed in the map with waiting works or works in processing.
 *
 * NOTE: If the work was not found, the work is finished or wasn't submitted to the cache daemon.
 *
 * @param key The key to identify a cache work.
 * @return True if the work was found, if not false is returned.
 */
bool CacheWorkManager::isWorkQueued(CacheWorkKey &key)
{
   bool found = false;

   SafeRWLock rwLock(&rwlockMaps, SafeRWLock_READ);               // R E A D L O C K

   auto searchResult = worksWaitToProcess->find(key);
   if(searchResult != worksWaitToProcess->end() )
   {
      found = true;
      goto unlock;
   }

   searchResult = worksInProcessing->find(key);
   if(searchResult != worksWaitToProcess->end() )
      found = true;

unlock:
   rwLock.unlock();                                               // U N L O C K - R/W

   return found;
}

/**
 * Checks if the work is listed in the map with waiting works or works in processing. It also checks
 * if the work reported an error.
 *
 * NOTE: If the work was not found, the work is finished or wasn't submitted to the cache daemon.
 *
 * @param key The key to identify a cache work.
 * @param waitForSubdirs True if the function should return if all flushes of the path and the given
 *        sub-directories are done.
 * @return DEEPER_RETVAL_SUCCESS if the work was processed successful else the errno from the
 *    processing of the work.
 */
int CacheWorkManager::checkSubmittedWork(CacheWorkKey &key, bool waitForSubdirs)
{
#ifdef BEEGFS_DEBUG
   bool found = false;
#endif

   int error = DEEPER_RETVAL_SUCCESS;

   CacheWorkFailedHashMapIter errorIter;

   SafeRWLock rwLock(&rwlockMaps, SafeRWLock_WRITE);                 // W R I T E L O C K

   CacheWorkHashMapIter searchResult = worksWaitToProcess->find(key);
   if(searchResult != worksWaitToProcess->end() )
   {
#ifdef BEEGFS_DEBUG
      found = true;
      Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path waits for processing: " +
      key.sourcePath);
#endif
      error = EBUSY;
      goto unlock;
   }

   searchResult = worksInProcessing->find(key);
   if(searchResult != worksWaitToProcess->end() )
   {
#ifdef BEEGFS_DEBUG
      found = true;
      Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path is in processing: " +
      key.sourcePath);
#endif
      error = EBUSY;
      goto unlock;
   }

   // check for error result of the work
   errorIter = worksFailed->find(key);
   if(errorIter != worksFailed->end() )
   {
#ifdef BEEGFS_DEBUG
      found = true;
      Logger::getLogger()->log(Log_DEBUG, __FUNCTION__, "Path processing failed: " +
   key.sourcePath);
#endif
      error = errorIter->second;
      worksFailed->erase(errorIter);
   }

unlock:
   rwLock.unlock();                                                     // U N L O C K - R/W

#ifdef BEEGFS_DEBUG
   if(!found)
   {
      Logger::getLogger()->log(Log_DEBUG, __FUNCTION__,
         "Path processing finished or not found: " + key.sourcePath);
   }
#endif

   return error;
}

/**
 * This function should be called if a work was processed. The work will be removed from the
 * worksInProcessing map and if a error happened the work is added to the error map.
 *
 * @param key The key to identify a cache work.
 * @param result The result value of the work which is finished.
 * @return True if a work was successful removed from the map.
 */
bool CacheWorkManager::handleFinishedWork(CacheWorkKey &key, int result)
{
   bool success = false;

   SafeRWLock rwLock(&rwlockMaps, SafeRWLock_WRITE);                 // W R I T E L O C K

   if(result != DEEPER_RETVAL_SUCCESS)
      worksFailed->insert(CacheWorkFailedHashMapVal(key, result) );

   size_t count = worksInProcessing->erase(key);

   if(count)
      success = true;

   rwLock.unlock();                                                  // U N L O C K - R/W

   return success;
}

/**
 * Aborts the given work and its children works.
 *
 * @param key The key to identify a cache work.
 * @return True if the stop could be initialized successful else false.
 */
bool CacheWorkManager::stopWork(CacheWorkKey &key)
{
   bool success = false;

   CacheWorkFailedHashMapIter searchResultFailed;

   SafeRWLock rwLock(&rwlockMaps, SafeRWLock_READ);                  // R E A D L O C K

   auto searchResult = worksWaitToProcess->find(key);
   if(searchResult != worksWaitToProcess->end() )
   {
      searchResult->second->stopWork();
      success = true;
      goto unlock;
   }

   searchResult = worksInProcessing->find(key);
   if(searchResult != worksWaitToProcess->end() )
   {
      searchResult->second->stopWork();
      success = true;
      goto unlock;
   }

   searchResultFailed = worksFailed->find(key);
   if(searchResultFailed != worksFailed->end() )
      success = true;

unlock:
   rwLock.unlock();                                                  // U N L O C K - R/W

   return success;
}

/**
 * Returns the checksum of the given work, if the calculation is finished.
 *
 * @param key The key to identify a cache work.
 * @return The CRC checksum or 0 if a checksum isn't available for the work.
 */
unsigned long CacheWorkManager::getCrcChecksum(CacheWorkKey &key)
{
   unsigned long checksum = 0;

   SafeRWLock rwLock(&rwlockMaps, SafeRWLock_WRITE);                 // W R I T E L O C K

   CacheWorkCrcSumHashMapIter valueIter = worksCrcChecksums->find(key);
   if(valueIter != worksCrcChecksums->end() )
   {
      checksum = valueIter->second;
      worksCrcChecksums->erase(valueIter);
   }

   rwLock.unlock();                                                  // U N L O C K - R/W

   return checksum;
}

/**
 * Adds a checksum to the map with finished crc works.
 *
 * @param key The key to identify a cache work.
 * @param The CRC checksum to add.
 */
void CacheWorkManager::addCrcChecksum(CacheWorkKey &key, unsigned long crcChecksum)
{
   SafeRWLock rwLock(&rwlockMaps, SafeRWLock_WRITE);                 // W R I T E L O C K

   CacheWorkCrcSumHashMapIter valueIter = worksCrcChecksums->find(key);
   if(valueIter != worksCrcChecksums->end() )
   {
      valueIter->second = crcChecksum;
   }
   else
   {
      worksCrcChecksums->insert(CacheWorkCrcSumHashMapVal(key, crcChecksum) );
   }

   rwLock.unlock();                                                  // U N L O C K - R/W
}

/**
 * Waits for a new work package. The cache worker should use this function.
 *
 * @return The new work package to process.
 */
Work* CacheWorkManager::waitForNewWork()
{
   Work* work = NULL;

   SafeMutexLock mutexLock(&newWorkMutex);         // L O C K

   while(!numPendingWorks)
      newWorkCond.wait(&newWorkMutex);

   if(!directWorkList->getIsEmpty() )
   {
      work = directWorkList->getAndPopNextWork();
      numPendingWorks--;
   }
   else if(!getIsEmpty() )
   {
      work = getAndPopNextWork();
      numPendingWorks--;
   }

   mutexLock.unlock();                             // U N L O C K

   return work;
}

/**
 * Waits for a new direct work package. The direct worker should use this function.
 *
 * @return A direct work package to process.
 */
Work* CacheWorkManager::waitForDirectWork()
{
   Work* work;

   SafeMutexLock mutexLock(&newWorkMutex);         // L O C K

   while(directWorkList->getIsEmpty() )
      newDirectWorkCond.wait(&newWorkMutex);

   work = directWorkList->getAndPopNextWork();
   numPendingWorks--;

   mutexLock.unlock();                             // U N L O C K

   return work;
}

/**
 * Generates stats for the health check.
 *
 * @outString The current stats of all lists and maps.
 * @return True if some stats to report then the outString contains some data, else outString is
 *         empty.
 */
bool CacheWorkManager::healthCheck(std::string& outString)
{
   std::ostringstream statsStream;

   SafeRWLock rwLock(&rwlockMaps, SafeRWLock_READ);      // R E A D L O C K

   if(fifo->size() )
   {
      statsStream << "* Queue len: " << fifo->size() << std::endl;
   }

   if(worksInProcessing->size() )
   {
      statsStream << "* Works in processing len: " << worksInProcessing->size() << std::endl;
   }

   if(worksFailed->size() )
   {
      statsStream << "* Works failed len: " << worksFailed->size() << std::endl;
   }

   if(worksCrcChecksums->size() )
   {
      statsStream << "* Works CRC checksums len: " << worksCrcChecksums->size() << std::endl;
   }

   rwLock.unlock();                                      // U N L O C K - R



   SafeMutexLock mutexLock(&newWorkMutex);         // L O C K

   if(numPendingWorks)
   {
      statsStream << "* Pending works (all queues): " << numPendingWorks << std::endl;
   }

   mutexLock.unlock();                             // U N L O C K


   outString = statsStream.str();

   return !outString.empty();
}
