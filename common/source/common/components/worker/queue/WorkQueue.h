#ifndef WORKQUEUE_H_
#define WORKQUEUE_H_

#include <common/components/worker/Work.h>
#include <common/threading/Mutex.h>
#include <common/threading/Condition.h>
#include <common/Common.h>

#include <mutex>

class WorkQueue
{
   public:
      WorkQueue() {}
      ~WorkQueue()
      {
         for(WorkListIter iter = workList.begin(); iter != workList.end(); iter++)
            delete(*iter);
      }

      Work* waitForNewWork()
      {
         std::lock_guard<Mutex> mutexLock(mutex);

         while(workList.empty() )
            newWorkCond.wait(&mutex);

         Work* work = *workList.begin();
         workList.pop_front();

         return work;
      }

      void addWork(Work* work)
      {
         std::lock_guard<Mutex> mutexLock(mutex);

         newWorkCond.signal();

         workList.push_back(work);
      }

   private:
      WorkList workList;

      Mutex mutex;
      Condition newWorkCond;
};

#endif /*WORKQUEUE_H_*/
