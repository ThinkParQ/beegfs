#ifndef COMMON_MUTEX_H_
#define COMMON_MUTEX_H_

#include "MutexException.h"
#include <common/system/System.h>
#include <common/Common.h>

class System;

class Mutex
{
   public:
      Mutex()
      {
         pthread_mutex_init(&mutex, NULL);
      }

      ~Mutex()
      {
         // may return:
         // * [EBUSY]  never returned by glibc, little useful info without a lockdep tool
         // * [EINVAL] never happens (mutex is properly initialized)
         pthread_mutex_destroy(&mutex);
      }

      Mutex(const Mutex&) = delete;
      Mutex(Mutex&&) = delete;
      Mutex& operator=(const Mutex&) = delete;
      Mutex& operator=(Mutex&&) = delete;

      /**
       * @throw MutexException
       */
      void lock()
      {
         int pthreadRes = pthread_mutex_lock(&mutex);

         if(unlikely(pthreadRes) )
            throw MutexException(System::getErrString(pthreadRes));
      }

      bool tryLock()
      {
         // may return:
         // * [EINVAL] never happens (mutex is properly initialized)
         // * [EBUSY]  not an error
         // * [EAGAIN] never happens (this mutex is not recursive)
         return pthread_mutex_trylock(&mutex) == 0;
      }

      void unlock()
      {
         // may return:
         // * [EINVAL] never happens (mutex is properly initialized)
         // * [EPERM]  never returned by glibc, little useful info without a lockdep tool
         pthread_mutex_unlock(&mutex);
      }

   private:
      pthread_mutex_t mutex;

   public:
      // getters & setters
      pthread_mutex_t* getMutex() {return &mutex;}
};

#endif /*COMMON_MUTEX_H_*/
