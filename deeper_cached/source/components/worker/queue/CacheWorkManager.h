#ifndef COMPONENTS_CACHEWORKMANAGER_H_
#define COMPONENTS_CACHEWORKMANAGER_H_


#include <common/components/worker/queue/AbstractWorkContainer.h>
#include <common/components/worker/queue/StreamListenerWorkQueue.h>
#include <common/threading/SafeMutexLock.h>
#include <common/threading/SafeRWLock.h>
#include <components/worker/DeeperWork.h>

#include <queue>



/**
 * A hash map for CacheWorkKeys (source path, CacheWorkType) to error codes (errno).
 */
typedef std::unordered_map<CacheWorkKey, int , CacheWorkKeyHash> CacheWorkFailedHashMap;
typedef CacheWorkFailedHashMap::iterator CacheWorkFailedHashMapIter;
typedef CacheWorkFailedHashMap::const_iterator CacheWorkFailedHashMapConstIter;
typedef CacheWorkFailedHashMap::value_type CacheWorkFailedHashMapVal;

/**
 * A hash map for CacheWorkKeys (source path, CacheWorkType) to CRC checksums.
 */
typedef std::unordered_map<CacheWorkKey, unsigned long, CacheWorkKeyHash> CacheWorkCrcSumHashMap;
typedef CacheWorkCrcSumHashMap::iterator CacheWorkCrcSumHashMapIter;
typedef CacheWorkCrcSumHashMap::const_iterator CacheWorkCrcSumHashMapConstIter;
typedef CacheWorkCrcSumHashMap::value_type CacheWorkCrcSumHashMapVal;



/**
 * A worker queue which manages all the works and the results, which must be processed.
 */
class CacheWorkManager : public AbstractWorkContainer, public StreamListenerWorkQueue
{
   public:
      CacheWorkManager();
      virtual ~CacheWorkManager();

      CacheWork* getAndPopNextWork();
      void addWork(Work* work, unsigned userID);
      int addWork(Work* work, bool allowAsSplit);
      int addSplitWork(Work* work);

      Work* waitForNewWork();
      Work* waitForDirectWork();

      bool isWorkQueued(CacheWorkKey &key);
      int checkSubmittedWork(CacheWorkKey &key, bool waitForSubdirs);
      bool handleFinishedWork(CacheWorkKey &key, int result);
      bool stopWork(CacheWorkKey &key);

      unsigned long getCrcChecksum(CacheWorkKey &key);
      void addCrcChecksum(CacheWorkKey &key, unsigned long crcChecksum);

      bool healthCheck(std::string& outString);


   private:
      std::queue<DeeperWork*>* fifo;        // the FIFO which contains all waiting works
      AbstractWorkContainer* directWorkList;
      CacheWorkHashMap* worksWaitToProcess; // lookup map for works which waits to be processed
      CacheWorkHashMap* worksInProcessing;  // lookup map for works which are in processing

      CacheWorkFailedHashMap* worksFailed; // lookup map for works which reports an error,
                                           // this can end up in heavy use of memory if the wait
                                           // function isn't called to get the error or the user
                                           //  program crashed
      CacheWorkCrcSumHashMap* worksCrcChecksums; // contains the CRC checksums of CRC works

      RWLock rwlockMaps;  // lock for all 4 maps. It is required to lock all maps because a work
                          // could be shifted from one map to an other map or removed. If a request
                          // happens during shifting a work a wrong state could be reported.
                          // worksWaitToProcess -> worksInProcessing ---> worksFailed
                          //                                          \--> worksCrcChecksums
                          //                                           \--> removed, successful work

      Mutex newWorkMutex;              // mutex for new work
      Condition newWorkCond;           // notification for new work
      Condition newDirectWorkCond;     // direct workers wait only on this condition

      uint64_t numPendingWorks; // length of direct+indirect list (not incl personal list)

   public:
      /**
       * Getter for the number of works which waits to be processed.
       *
       * @return The number of the works which are waiting.
       */
      size_t getSize()
      {
         SafeRWLock rwLock(&rwlockMaps, SafeRWLock_READ);      // R E A D L O C K

         size_t retVal = fifo->size();

         rwLock.unlock();                                      // U N L O C K - R/W

         return retVal;
      }

      /***
       * Checks if the map of works which waits to be processed is empty.
       *
       * @return True it the map is empty.
       */
      bool getIsEmpty()
      {
         SafeRWLock rwLock(&rwlockMaps, SafeRWLock_READ);      // R E A D L O C K

         bool retVal = fifo->empty();

         rwLock.unlock();                                      // U N L O C K - R/W

         return retVal;
      }

      /**
       * Getter for the number of works which are in processing.
       *
       * @return The number of the works.
       */
      size_t getSizeOfWorksInProcessing()
      {
         SafeRWLock rwLock(&rwlockMaps, SafeRWLock_READ);      // R E A D L O C K

         size_t retVal = worksInProcessing->size();

         rwLock.unlock();                                      // U N L O C K - R/W

         return retVal;
      }

      /***
       * Checks if the map of works which are in processing is empty.
       *
       * @return True it the map is empty.
       */
      bool getIsWorksInProcessingEmpty()
      {
         SafeRWLock rwLock(&rwlockMaps, SafeRWLock_READ);      // R E A D L O C K

         bool retVal = worksInProcessing->empty();

         rwLock.unlock();                                      // U N L O C K - R/W

         return retVal;
      }

      /**
       * Getter for the number of works which reports an error during the processing.
       *
       * @return The number of the works.
       */
      size_t getSizeOfWorksFailed()
      {
         SafeRWLock rwLock(&rwlockMaps, SafeRWLock_READ);      // R E A D L O C K

         size_t retVal = worksFailed->size();

         rwLock.unlock();                                      // U N L O C K - R/W

         return retVal;
      }

      /***
       * Checks if the map of works which reports an error during the processing is empty.
       *
       * @return True it the map is empty.
       */
      bool getIsWorksFailedEmpty()
      {
         SafeRWLock rwLock(&rwlockMaps, SafeRWLock_READ);      // R E A D L O C K

         bool retVal = worksFailed->empty();

         rwLock.unlock();                                      // U N L O C K - R/W

         return retVal;
      }

      /**
       * Getter for the number of works with a calculated CRC checksum.
       *
       * @return The number of the works.
       */
      size_t getSizeOfWorksCrcChecksums()
      {
         SafeRWLock rwLock(&rwlockMaps, SafeRWLock_READ);      // R E A D L O C K

         size_t retVal = worksCrcChecksums->size();

         rwLock.unlock();                                      // U N L O C K - R/W

         return retVal;
      }

      /***
       * Checks if the map of works with a calculated CRC checksum is empty.
       *
       * @return True it the map is empty.
       */
      bool getIsWorksCrcChecksumsEmpty()
      {
         SafeRWLock rwLock(&rwlockMaps, SafeRWLock_READ);      // R E A D L O C K

         bool retVal = worksCrcChecksums->empty();

         rwLock.unlock();                                      // U N L O C K - R/W

         return retVal;
      }

      /**
       * Reports the statistics of the queue in a string.
       *
       * @param outStats String with the statistics of the queue.
       */
      void getStatsAsStr(std::string& outStats)
      {
         std::ostringstream statsStream;

         statsStream << "* Queue type: CacheWorkManager" << std::endl;
         statsStream << "* Queue len: " << getSize() << std::endl;
         statsStream << "* Works in processing len: " << getSizeOfWorksInProcessing() << std::endl;
         statsStream << "* Works failed len: " << getSizeOfWorksFailed() << std::endl;
         statsStream << "* Works CRC checksums len: " << getSizeOfWorksCrcChecksums() << std::endl;

         outStats = statsStream.str();
      }

      /**
       * Adds a direct work to the queue.
       *
       * @param work The work to add.
       * @param userID The user ID of the work package. Is ignored in this implementation.
       */
      virtual void addDirectWork(Work* work,
         unsigned userID = STREAMLISTENERWORKQUEUE_DEFAULT_USERID)
      {
         SafeMutexLock mutexLock(&newWorkMutex);               // L O C K

         directWorkList->addWork(work, userID);

         numPendingWorks++;

         newWorkCond.signal();
         newDirectWorkCond.signal();

         mutexLock.unlock();                                   // U N L O C K - R/W
      }

      /**
       * Just a wrapper for addWork(). It is required for the StreamListenerV2.
       *
       * @param work The work to add.
       * @param userID The user ID of the work package. Is ignored in this implementation.
       */
      virtual void addIndirectWork(Work* work,
         unsigned userID = STREAMLISTENERWORKQUEUE_DEFAULT_USERID)
      {
         addWork(work, userID);
      }
};

#endif /* COMPONENTS_CACHEWORKMANAGER_H_ */
