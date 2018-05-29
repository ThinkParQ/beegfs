#ifndef RWLOCK_H_
#define RWLOCK_H_

#include <common/system/System.h>
#include <common/threading/Atomics.h>
#include <common/app/log/LogContext.h>
#include <common/Common.h>
#include "RWLockException.h"


#define RWLOCK_READERS_SKIP_LIMIT   8 /* number of queued reader ignores before writers yield */

/* Retries for races, when we give up a read-lock in order to gain a write lock.
 * If pthread would allow a lock-upgrade we wouldn't need this at all. Therefore 2 tries, one as
 * read-lock and another one as write-lock. In between those we need to unlock() and therefore
 * introduce a race that needs to be handled by the corresponding code. */
#define RWLOCK_LOCK_UPGRADE_RACY_RETRIES 2


enum RWLockLockType
{
   RWLockType_UNSET = -1,
   RWLockType_READ  =  1,
   RWLockType_WRITE =  2
};

/**
 * Standard rwlocks with writer preference and special checks to avoid reader starvation.
 *
 * As standard pthread rwlocks cannot avoid either writer or reader starvation in situations of
 * high concurrency, we set writer preference and increase a counter for waiting (queued) readers.
 * A few writers are granted even when there are readers waiting, but at some point the writers will
 * yield and allow the readers to get their hands on the lock.
 */
class RWLock
{
   public:
      RWLock()
      {
         /* note: most impls of libc on Linux seem to prefer readers by default, which can lead
            to writer starvation. hence we need to set writer preference explicitly (but
            unfortunately, there is only a non-portable method for that). */

         pthread_rwlockattr_t attr;

         int attrInitRes = pthread_rwlockattr_init(&attr);
         if(unlikely(attrInitRes) )
            throw RWLockException("RWLock attribs init error: " +
               System::getErrString(attrInitRes) );

         /* note: always use PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP here and don't switch to
            PTHREAD_RWLOCK_PREFER_WRITER_NP, because that one is buggy and won't get fixed */

         int kindRes = pthread_rwlockattr_setkind_np(&attr,
            PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
         if(unlikely(kindRes) )
            throw RWLockException("RWLock setkind error: " + System::getErrString(kindRes) );

         int lockInitRes = pthread_rwlock_init(&rwlock, &attr);
         if(unlikely(lockInitRes) )
            throw RWLockException("RWLock lock init error: " + System::getErrString(lockInitRes) );

         lockType = RWLockType_UNSET;
      }

      ~RWLock()
      {
         // may return
         // * [EBUSY]  never returned by glibc, little useful info without a lockdep tool
         // * [EINVAL] never happens (mutex is properly initialized)
         pthread_rwlock_destroy(&rwlock);
      }

      RWLock(RWLock&&) = delete;
      RWLock(const RWLock&) = delete;
      RWLock& operator=(RWLock&&) = delete;
      RWLock& operator=(const RWLock&) = delete;

      /**
       * @throw RWLockException
       */
      void writeLock()
      {
         const char* logContext = "RWLock::writeLock";
         int pthreadRes;

         if(numQueuedReaders.read() )
         {
            if(numSkippedReaders.read() > RWLOCK_READERS_SKIP_LIMIT)
            {
               // Grab (and release) a read lock to avoid reader starvation. That way other
               // waiting readers also will get through.

               pthreadRes = pthread_rwlock_rdlock(&rwlock);
               if(unlikely(pthreadRes) )
                  throw RWLockException(System::getErrString(pthreadRes) );

               pthreadRes = pthread_rwlock_unlock(&rwlock);
               if(unlikely(pthreadRes) )
                  throw RWLockException(System::getErrString(pthreadRes) );
            }
            else
            {
               /* not every single waiting reader indicates starvation, so we skip
                  anti-starvaion mechanism a few times for better performance */

               numSkippedReaders.increase();
            }
         }

         pthreadRes = pthread_rwlock_wrlock(&rwlock);
         if(unlikely(pthreadRes) )
         {
            std::string sysErrStr = System::getErrString(pthreadRes);
            LogContext(logContext).logErr("Failed to get lock: " + sysErrStr);
            throw RWLockException(sysErrStr);
         }

         lockType = RWLockType_WRITE;
      }

      /**
       * @throw RWLockException
       */
      void readLock()
      {
         const char* logContext = "RWLock::readLock";

         if(!tryReadLock() )
         {
            // inform writers about waiting readers, so they can avoid starvation
            numQueuedReaders.increase();

            int pthreadRes = pthread_rwlock_rdlock(&rwlock);
            if(unlikely(pthreadRes) )
            {
               std::string sysErrStr = System::getErrString(pthreadRes);
               LogContext(logContext).logErr("Failed to get lock: " + sysErrStr);
               LogContext(logContext).logBacktrace();
               throw RWLockException(sysErrStr);
            }

            numQueuedReaders.decrease();
            numSkippedReaders.setZero();

            lockType = RWLockType_READ;
         }
      }

      /**
       * @timeOutSecs number of seconds to wait before giving up aquiring this lock
       *
       * Note: Only use this method for debugging purposes!
       */
      int timedReadLock(size_t timeOutSecs)
      {
         if (!tryReadLock() )
         {
            // inform writers about waiting readers, so they can avoid starvation
            numQueuedReaders.increase();

            // C99 style: { .tv_sec = timeOutSecs, .tv_nsec = 0 };
            struct timespec timeSpec = { (time_t) timeOutSecs, 0 };

            int pthreadRes = pthread_rwlock_timedrdlock(&rwlock, &timeSpec);
            if(!pthreadRes)
            {  // go the lock
               numQueuedReaders.decrease();
               numSkippedReaders.setZero();

               lockType = RWLockType_READ;
            }

            return pthreadRes;
         }

         // tryReadLock() succeeded
         return 0;
      }

      bool tryWriteLock()
      {
         // may return
         // * [EINVAL] never happens (rwlock is properly initialized)
         // * [EBUSY]  not an error
         if (pthread_rwlock_trywrlock(&rwlock) != 0)
            return false;

         lockType = RWLockType_WRITE;
         return true;
      }

      bool tryReadLock()
      {
         // may return
         // * [EINVAL] never happens (rwlock is properly initialized)
         // * [EBUSY]  not an error
         // * [EAGAIN] not an error
         if (pthread_rwlock_tryrdlock(&rwlock) != 0)
            return false;

         lockType = RWLockType_READ;
         return true;
      }

      void unlock()
      {
         lockType = RWLockType_UNSET;

         // may return:
         // * [EINVAL] never happens (rwlock is properly initialized)
         // * [EPERM]  never returned by glibc, little useful info without a lockdep tool
         pthread_rwlock_unlock(&rwlock);
      }


   private:
      pthread_rwlock_t rwlock;
      AtomicUInt32 numQueuedReaders; // number of readers waiting for lock
      AtomicUInt32 numSkippedReaders; // number of write locks that ignored queued readers

      RWLockLockType lockType;

   public:
      // getters & setters
      pthread_rwlock_t* getRWLock()
      {
         return &rwlock;
      }

      /**
       * Do we have the read-write lock?
       */
      bool isRWLocked()
      {
         if (lockType == RWLockType_WRITE)
            return true;

         return false;
      }

      /**
       * Do we have the read-only lock?
       */
      bool isROLocked()
      {
         if (lockType == RWLockType_READ)
            return true;

         return false;
      }

};

#endif /* RWLOCK_H_ */
