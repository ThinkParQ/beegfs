#ifndef WORKQUEUE_H_
#define WORKQUEUE_H_

#include <common/components/worker/Work.h>
#include <common/threading/Mutex.h>
#include <common/threading/SafeMutexLock.h>
#include <common/threading/Condition.h>
#include <common/Common.h>

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
         SafeMutexLock mutexLock(&mutex);
         
         while(workList.empty() )
            newWorkCond.wait(&mutex);
            
         Work* work = *workList.begin();
         workList.pop_front();
         
         mutexLock.unlock();
         
         return work;
      }
      
      void addWork(Work* work)
      {
         SafeMutexLock mutexLock(&mutex);
         
         newWorkCond.signal();

         workList.push_back(work);

         mutexLock.unlock();
      }
      
   private:
      WorkList workList;
      
      Mutex mutex;
      Condition newWorkCond;
};

#endif /*WORKQUEUE_H_*/
