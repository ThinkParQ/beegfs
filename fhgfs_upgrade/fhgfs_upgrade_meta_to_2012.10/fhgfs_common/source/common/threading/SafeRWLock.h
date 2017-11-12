#ifndef SAFERWLOCK_H_
#define SAFERWLOCK_H_

#include "RWLock.h"
#include "PThread.h"

enum SafeRWLockType
   { SafeRWLock_READ = 0, SafeRWLock_WRITE = 1 };

/**
 * This class does not replace a rwlock - it is just a wrapper around a rwlock.
 * This wrapper is not thread-safe (e.g. you cannot use the same shared SafeRWLock instance in
 * multiple threads). Instead, it is designed to be instantiated on the stack.
 */
class SafeRWLock
{
   public:

      /**
       * Already takes the lock.
       */
      SafeRWLock(RWLock* rwlock, SafeRWLockType lockType) : rwlock(rwlock), locked(true)
      {
         if(lockType == SafeRWLock_READ)
         {
            this->rwlock->readLock();
         }
         else
         {
            this->rwlock->writeLock();
         }
      }

      /**
       * Does not take the lock yet.
       */
      SafeRWLock(RWLock* rwlock) :rwlock(rwlock), locked(false) {}

      ~SafeRWLock()
      {
         #ifdef DEBUG_MUTEX_LOCKING

            if(this->locked)
            {
               errRWLockStillLocked();
               this->rwlock->unlock();
            }

         #else

            // nothing to be done here

         #endif // DEBUG_MUTEX_LOCKING
      }

      void unlock()
      {
         #ifdef DEBUG_MUTEX_LOCKING

            if(!this->locked)
               errRWLockAlreadyUnlocked();
            else
            {
               this->locked   = false;
               this->rwlock->unlock();
            }

         #else

            rwlock->unlock();

         #endif // DEBUG_MUTEX_LOCKING
      }

      void lock(SafeRWLockType lockType)
      {
         #ifdef DEBUG_MUTEX_LOCKING

            if(this->locked)
               errRWLockAlreadyLocked();
            else
            {
               this->locked = true;

               if(lockType == SafeRWLock_READ)
                  this->rwlock->readLock();
               else
                  this->rwlock->writeLock();
            }

         #else

            if(lockType == SafeRWLock_READ)
               this->rwlock->readLock();
            else
               this->rwlock->writeLock();

         #endif // DEBUG_MUTEX_LOCKING
      }

      void logDebug(std::string msg);

   private:
      RWLock* rwlock;
      bool locked; // note: this value is updated in debug mode only

      void errRWLockStillLocked();
      void errRWLockAlreadyUnlocked();
      void errRWLockAlreadyLocked();


};

#endif /* SAFERWLOCK_H_ */
