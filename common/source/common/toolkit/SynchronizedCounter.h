#ifndef SYNCHRONIZEDCOUNTER_H_
#define SYNCHRONIZEDCOUNTER_H_

#include <common/Common.h>
#include <common/threading/Mutex.h>
#include <common/threading/Condition.h>

#include <mutex>

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
         const std::lock_guard<Mutex> lock(mutex);

         while(count != waitCount)
            cond.wait(&mutex);
      }

      bool timedWaitForCount(unsigned waitCount, int timeoutMS)
      {
         const std::lock_guard<Mutex> lock(mutex);

         if (count != waitCount)
            if ( (!cond.timedwait(&mutex, timeoutMS)) ||  (count < waitCount) )
               return false;

         return true;
      }

      void incCount()
      {
         const std::lock_guard<Mutex> lock(mutex);

         count++;

         cond.broadcast();
      }

      void resetUnsynced()
      {
         count = 0;
      }
};

#endif /*SYNCHRONIZEDCOUNTER_H_*/
