#ifndef CONDITION_H_
#define CONDITION_H_

#include <common/system/System.h>
#include <common/Common.h>
#include "MutexException.h"
#include "Mutex.h"

#include <sys/time.h>


class System;

class Condition
{
   public:
      Condition()
      {
         pthread_cond_init(&condition, NULL);
      }
      
      ~Condition()
      {
         int pthreadRes = pthread_cond_destroy(&condition);
         
         if(unlikely(pthreadRes) )
            throw MutexException(System::getErrString(pthreadRes) );
      }
      
      void signal()
      {
         pthread_cond_signal(&condition);
      }
      
      void broadcast()
      {
         pthread_cond_broadcast(&condition);
      }
      
      void wait(Mutex* lockedMutex)
      {
         pthread_cond_wait(&condition, lockedMutex->getMutex() );
      }
      
      bool timedwait(Mutex* lockedMutex, const int timeoutMS)
      {
         struct timeval now;
         struct timespec timeout;
         gettimeofday(&now, NULL);
         
         timeout.tv_sec = now.tv_sec + timeoutMS/1000;
         timeout.tv_nsec = now.tv_usec * 1000 + (timeoutMS % 1000) * 1000000;
         
         timeout.tv_sec = timeout.tv_sec + timeout.tv_nsec / (1000*1000*1000);
         timeout.tv_nsec = timeout.tv_nsec % (1000*1000*1000);

         int pthreadRes = pthread_cond_timedwait(&condition, lockedMutex->getMutex(), &timeout);
         if(!pthreadRes)
            return true;
         if(pthreadRes == ETIMEDOUT)
            return false;

         throw MutexException(System::getErrString(pthreadRes) );
      }
   
   private:
      pthread_cond_t condition;
};

#endif /*CONDITION_H_*/
