#ifndef SYNCHRONIZEDCOUNTER_H_
#define SYNCHRONIZEDCOUNTER_H_

#include <common/Common.h>
#include <common/threading/Mutex.h>
#include <common/threading/SafeMutexLock.h>
#include <common/threading/Condition.h>

class SynchronizedCounter
{
   public:
      SynchronizedCounter()
      {
         this->count = 0;
      }

   private:
      unsigned count;

      Mutex mutex;
      Condition cond;

   public:
      // inliners
      void waitForCount(unsigned waitCount)
      {
         SafeMutexLock mutexLock(&mutex);

         while(count != waitCount)
            cond.wait(&mutex);

         mutexLock.unlock();
      }

      bool timedWaitForCount(unsigned waitCount, int timeoutMS)
      {
         bool retVal = true;

         SafeMutexLock mutexLock(&mutex);

         if (count != waitCount)
            if ( (!cond.timedwait(&mutex, timeoutMS)) ||  (count < waitCount) )
               retVal = false;

         mutexLock.unlock();

         return retVal;
      }

      void incCount()
      {
         SafeMutexLock mutexLock(&mutex);

         count++;

         cond.broadcast();

         mutexLock.unlock();
      }

      void resetUnsynced()
      {
         count = 0;
      }
};

#endif /*SYNCHRONIZEDCOUNTER_H_*/
