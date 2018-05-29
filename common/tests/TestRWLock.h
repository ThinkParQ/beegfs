#ifndef TESTRWLOCK_H_
#define TESTRWLOCK_H_


#include <common/threading/Condition.h>
#include <common/threading/RWLock.h>
#include <common/threading/PThread.h>

#include <gtest/gtest.h>
#include <unistd.h>


// configuration for the tests
#define TestRWLock_THREAD_COUNT             50
#define TestRWLock_LOCK_DELAY_MS            1000
#define TestRWLock_SLEEP_TIME_S             3
#define TestRWLock_SLEEP_TIME_MS            (TestRWLock_SLEEP_TIME_S * 1000)
#define TestRWLock_RANDOM_SLEEP_TIME_MIN_MS 1000
#define TestRWLock_RANDOM_SLEEP_TIME_MAX_MS 4000
#define TestRWLock_RANDOM_LOCK_DELAY_MIN_MS 2000
#define TestRWLock_RANDOM_LOCK_DELAY_MAX_MS 8000
#define TestRWLock_SINGLE_TIMEOUT_MS        15000
#define TestRWLock_MULTI_TIMEOUT_MS         (TestRWLock_SINGLE_TIMEOUT_MS * TestRWLock_THREAD_COUNT)


class TestRWLock: public ::testing::Test
{
   protected:
      /**
       * Thread for testing the concurrent access to a shared resource which is secured by
       * an rwlock
       */
      class TestLockThread: public PThread
      {
         public:
            TestLockThread() : PThread("RWLockTester")
            {
               this->lock = NULL;

               this->sleepTimeMS = TestRWLock_SLEEP_TIME_MS;
               this->lockDelayMS = TestRWLock_LOCK_DELAY_MS;

               this->doReadLock = true;
               this->doTry = false;

               this->lockSuccess = false;
               this->unlockSuccess = false;

               this->lockTimestamp.setToNow();
               this->unlockTimestamp.setToNow();
               this->initTimestamp.setToNow();
            }

            TestLockThread(RWLock* lock, bool doReadLock, bool doTry) : PThread("RWLockTester")
            {
               this->lock = lock;

               this->sleepTimeMS = TestRWLock_SLEEP_TIME_MS;
               this->lockDelayMS = TestRWLock_LOCK_DELAY_MS;

               this->doReadLock = doReadLock;
               this->doTry = doTry;

               this->lockSuccess = false;
               this->unlockSuccess = false;

               this->lockTimestamp.setToNow();
               this->unlockTimestamp.setToNow();
               this->initTimestamp.setToNow();
            }

            TestLockThread(RWLock* lock, bool doReadLock, bool doTry, int sleepTimeMS,
               int lockDelayMS) : PThread("RWLockTester")
            {
               this->lock = lock;

               this->sleepTimeMS = sleepTimeMS;
               this->lockDelayMS = lockDelayMS;

               this->doReadLock = doReadLock;
               this->doTry = doTry;

               this->lockSuccess = false;
               this->unlockSuccess = false;

               this->lockTimestamp.setToNow();
               this->unlockTimestamp.setToNow();
               this->initTimestamp.setToNow();
            }

         protected:

         private:
            RWLock* lock;           // the rwlock which all threads are use

            int sleepTimeMS;        // amount of to time to wait before start locking the rwlock
            int lockDelayMS;        // the delay between the lock and the unlock

            bool doReadLock;        // true if this thread tries a read lock
            bool doTry;             // true if a try*Lock will be used

            bool lockSuccess;       // true if the lock was successful
            bool unlockSuccess;     // true if the unlock was successful

            Time lockTimestamp;     // the timestamp of the successful lock
            Time unlockTimestamp;   // the timestamp of the successful unlock
            Time initTimestamp;     // the timestamp at the end of the initialization

         public:
            //public inliner

            void init(RWLock* lock, bool doReadLock, bool doTry, int sleepTimeMS, int lockDelayMS)
            {
               this->lock = lock;

               this->sleepTimeMS = sleepTimeMS;
               this->lockDelayMS = lockDelayMS;

               this->doReadLock = doReadLock;
               this->doTry = doTry;

               this->lockTimestamp.setToNow();
               this->unlockTimestamp.setToNow();
               this->initTimestamp.setToNow();
            }

            void copy(TestLockThread* origin)
            {
               this->lock = origin->lock;

               this->sleepTimeMS = origin->sleepTimeMS;
               this->lockDelayMS = origin->lockDelayMS;

               this->doReadLock = origin->doReadLock;
               this->doTry = origin->doTry;

               this->lockSuccess = origin->lockSuccess;
               this->unlockSuccess = origin->unlockSuccess;

               this->lockTimestamp = Time(origin->lockTimestamp);
               this->unlockTimestamp = Time(origin->unlockTimestamp);
               this->initTimestamp = Time(origin->initTimestamp);
            }

            bool getDoReadLock()
            {
               return this->doReadLock;
            }

            bool getSleepTimeMS()
            {
               return this->sleepTimeMS;
            }

            bool getLockSuccess()
            {
               return this->lockSuccess;
            }

            bool getUnlockSuccess()
            {
               return this->unlockSuccess;
            }

            Time getLockTimestamp()
            {
               return this->lockTimestamp;
            }

            Time getUnlockTimestamp()
            {
               return this->unlockTimestamp;
            }

            void run()
            {
               // start delay for random start of the threads, this is needed for a random
               // execution order of the test threads, is necessary for a good test
               sleepMS(this->lockDelayMS);

               if (this->doReadLock)
               {
                  if(this->doTry)
                  {
                     this->lockSuccess = this->lock->tryReadLock();
                     if (this->lockSuccess)
                        this->lockTimestamp.setToNow();
                  }
                  else
                  {
                     this->lock->readLock();
                     this->lockTimestamp.setToNow();
                     this->lockSuccess = true;
                  }
               }
               else
               {
                  if(this->doTry)
                  {
                     this->lockSuccess = this->lock->tryWriteLock();
                     if (this->lockSuccess)
                        this->lockTimestamp.setToNow();
                  }
                  else
                  {
                     this->lock->writeLock();
                     this->lockTimestamp.setToNow();
                     this->lockSuccess = true;
                  }
               }

               // delay between lock and unlock for random execution order of the locking,
               // is necessary for a good test
               sleepMS(this->sleepTimeMS);

               if (this->lockSuccess)
               {
                  this->unlockTimestamp.setToNow();
                  this->lock->unlock();
                  this->unlockSuccess = true;
               }
            }
      };

      void sortThreadsInLockTimestamp(TestLockThread* threads, Time* startTime);
      void checkRandomRuntime(TestLockThread* threads, int runtimeMS);
      void checkRandomExecutionOrder(TestLockThread* threads);
      void checkRandomExecutionOrderReader(TestLockThread* threads, int threadID);
      void checkRandomExecutionOrderWriter(TestLockThread* threads, int threadID);
};

#endif /* TESTRWLOCK_H_ */
