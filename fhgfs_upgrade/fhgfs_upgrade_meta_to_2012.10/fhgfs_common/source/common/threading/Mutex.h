#ifndef MUTEX_H_
#define MUTEX_H_

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
      
      /**
       * @throw MutexException
       */
      ~Mutex()
      {
         int pthreadRes = pthread_mutex_destroy(&mutex);

         if(unlikely(pthreadRes) )
            throw MutexException(System::getErrString(pthreadRes));
      }
      
      /**
       * @throw MutexException
       */
      void lock()
      {
         int pthreadRes = pthread_mutex_lock(&mutex);
         
         if(unlikely(pthreadRes) )
            throw MutexException(System::getErrString(pthreadRes));
      }

      /**
       * @throw MutexException
       */
      bool tryLock()
      {
         int pthreadRes = pthread_mutex_trylock(&mutex);
         if(!pthreadRes)
            return true;
         if(pthreadRes == EBUSY)
            return false;

         throw MutexException(System::getErrString(pthreadRes));
      }

      /**
       * @throw MutexException
       */
      void unlock()
      {
         int pthreadRes = pthread_mutex_unlock(&mutex);
         
         if(unlikely(pthreadRes) )
            throw MutexException(System::getErrString(pthreadRes));
      }
   
   private:
      pthread_mutex_t mutex;
      
   public:
      // getters & setters
      pthread_mutex_t* getMutex() {return &mutex;}
};

#endif /*MUTEX_H_*/
