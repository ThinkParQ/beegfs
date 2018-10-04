#ifndef OPEN_THREAD_H_
#define OPEN_THREAD_H_

#include <common/Common.h>
#include <common/threading/Mutex.h>
#include <common/threading/Condition.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/Time.h>


#define BEEGFS_TASK_COMM_LEN   16  /* task command name length */


struct task_struct; // forward declaration


struct Thread;
typedef struct Thread Thread;


typedef void (*ThreadRoutine)(Thread* this);


extern void Thread_init(Thread* this, const char* initialThreadName, ThreadRoutine threadRoutine);
extern void Thread_uninit(Thread* this);

extern bool Thread_start(Thread* this);
extern void Thread_join(Thread* this);
extern bool Thread_timedjoin(Thread* this, int timeoutMS);
extern void Thread_selfTerminate(Thread* this);
extern void Thread_sleep(int timeoutMS);

extern int __Thread_runStatic(void* data);

// inliners
static inline bool _Thread_waitForSelfTerminateOrder(Thread* this, int timeoutMS);

// getters & setters
extern const char* Thread_getName(Thread* this);
extern void Thread_getCurrentThreadName(char* outBuf);
static inline bool Thread_getSelfTerminate(Thread* this);
static inline void __Thread_setSelfTerminated(Thread* this);


struct Thread
{
   bool shallSelfTerminate;
   Mutex selfTerminateMutex;
   Condition shallSelfTerminateCond;

   bool isSelfTerminated;
   Condition isSelfTerminatedChangeCond;

   char initialThreadName[BEEGFS_TASK_COMM_LEN];

   struct task_struct* threadData;

   ThreadRoutine threadRoutine;
};


bool _Thread_waitForSelfTerminateOrder(Thread* this, int timeoutMS)
{
   bool shallSelfTerminate;

   Mutex_lock(&this->selfTerminateMutex);

   if(!this->shallSelfTerminate)
   {
      Condition_timedwaitInterruptible(
         &this->shallSelfTerminateCond, &this->selfTerminateMutex, timeoutMS);
   }

   shallSelfTerminate = this->shallSelfTerminate;

   Mutex_unlock(&this->selfTerminateMutex);

   return shallSelfTerminate;
}


bool Thread_getSelfTerminate(Thread* this)
{
   bool shallSelfTerminate;

   Mutex_lock(&this->selfTerminateMutex);

   shallSelfTerminate = this->shallSelfTerminate;

   Mutex_unlock(&this->selfTerminateMutex);

   return shallSelfTerminate;
}

void __Thread_setSelfTerminated(Thread* this)
{
   Mutex_lock(&this->selfTerminateMutex);

   this->isSelfTerminated = true;

   Condition_broadcast(&this->isSelfTerminatedChangeCond);

   Mutex_unlock(&this->selfTerminateMutex);
}

#endif /*OPEN_THREAD_H_*/
