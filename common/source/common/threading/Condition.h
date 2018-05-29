#ifndef CONDITION_H_
#define CONDITION_H_

#include <common/system/System.h>
#include <common/Common.h>
#include <common/toolkit/Time.h>
#include "MutexException.h"
#include "Mutex.h"

#include <sys/time.h>


class System;

class Condition
{
   public:
      Condition()
      {
         pthread_cond_init(&this->condition, &condAttr);
      }

      ~Condition()
      {
         int condRes = pthread_cond_destroy(&condition);
         if(unlikely(condRes) )
            std::terminate();
      }

      Condition(const Condition&) = delete;
      Condition(Condition&&) = delete;
      Condition& operator=(const Condition&) = delete;
      Condition& operator=(Condition&&) = delete;

       static bool initStaticCondAttr();
      static bool destroyStaticCondAttr();
      static bool testClockID();

   private:
      pthread_cond_t condition;
      static pthread_condattr_t condAttr;

      /* In principle we could use Time::getClockID(), but unfortunately, glibc developers
       * forgot to support recent clockIDs. So we still let the Time:: class use the faster
       * CLOCK_MONOTONIC_COARSE variant, but will auto-fail back to the slower version here
       * (Well, maybe we think about to use another libc, such as musl, which does not have the
       * glibc limitation).
       */
      static clockid_t clockID;


   public:

      // inliners

      bool timedwait(Mutex* lockedMutex, const int timeoutMS)
      {
         Time timeout; // initialized with current time
         timeout.addMS(timeoutMS);

         struct timespec timeoutTimeSpec;
         timeout.getTimeSpec(&timeoutTimeSpec);

         int pthreadRes = pthread_cond_timedwait(&condition, lockedMutex->getMutex(),
            &timeoutTimeSpec);
         if(!pthreadRes)
            return true;
         if(pthreadRes == ETIMEDOUT)
            return false;

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

};

#endif /*CONDITION_H_*/
