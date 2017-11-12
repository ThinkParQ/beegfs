#ifndef SAFEMUTEXLOCK_H_
#define SAFEMUTEXLOCK_H_

#include "Mutex.h"
#include "PThread.h"

/**
 * This class does not replace a mutex - it is just a wrapper around a mutex.
 * This wrapper is not thread-safe (e.g. you cannot use the same shared SafeMutexLock instance in
 * multiple threads). Instead, it is designed to be instantiated on the stack.
 */
class SafeMutexLock
{
   public:
      SafeMutexLock(Mutex* mutex) : mutex(mutex), locked(true)
      {
         this->mutex->lock();
      }

      ~SafeMutexLock()
      {
         #ifdef DEBUG_MUTEX_LOCKING

            if(locked)
            {
               errMutexStillLocked();
               mutex->unlock();
            }

         #else

            // nothing to be done here

         #endif // DEBUG_MUTEX_LOCKING
      }

      SafeMutexLock() = delete;
      SafeMutexLock(SafeMutexLock const&) = delete;
      SafeMutexLock(SafeMutexLock&&) = delete;
      SafeMutexLock& operator=(SafeMutexLock const&) = delete;
      SafeMutexLock& operator=(SafeMutexLock&&) = delete;

      void unlock()
      {
         #ifdef DEBUG_MUTEX_LOCKING

            if(!locked)
               errMutexAlreadyUnlocked();
            else
            {
               locked = false;
               mutex->unlock();
            }

         #else

            mutex->unlock();

         #endif // DEBUG_MUTEX_LOCKING
      }

      void relock()
      {
         #ifdef DEBUG_MUTEX_LOCKING

            if(locked)
               errMutexAlreadyLocked();
            else
            {
               locked = true;
               mutex->lock();
            }

         #else

            mutex->lock();

         #endif // DEBUG_MUTEX_LOCKING
      }

      void logDebug(std::string msg);

   private:
      Mutex* mutex;
      bool locked; // note: this value is updated in debug mode only

      void errMutexStillLocked();
      void errMutexAlreadyUnlocked();
      void errMutexAlreadyLocked();


};

#endif /*SAFEMUTEXLOCK_H_*/
