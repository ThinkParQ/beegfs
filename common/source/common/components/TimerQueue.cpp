#include "TimerQueue.h"

#include <common/toolkit/StringTk.h>

#include <mutex>


void TimerWorkList::enqueue(std::function<void ()> fn)
{
   std::unique_lock<Mutex> lock(mutex);

   available.push_back(std::move(fn));
   condition.signal();
}

std::pair<bool, std::function<void ()>> TimerWorkList::deque(int timeoutMS)
{
   std::unique_lock<Mutex> lock(mutex);

   if (available.empty())
   {
      if (!condition.timedwait(&mutex, timeoutMS))
         return {false, {}};
   }

   if (available.empty())
      return {false, {}};

   std::function<void ()> fn = std::move(available.front());
   available.pop_front();
   return {true, std::move(fn)};
}

size_t TimerWorkList::waitingItems() const
{
   std::unique_lock<Mutex> lock(mutex);

   return available.size();
}



TimerWorker::TimerWorker(TimerQueue& queue, TimerWorkList& workList, unsigned id, bool keepAlive):
   PThread("TimerWork/" + StringTk::uintToStr(id)), queue(&queue), workList(&workList),
   keepAlive(keepAlive), id(id), state(State_STOPPED)
{
}

TimerWorker::~TimerWorker()
{
   stop();
}

void TimerWorker::start()
{
   if (state.read() == State_RUNNING)
      return;

   if (state.read() == State_STOPPING)
      join();

   state.set(State_RUNNING);

   try
   {
      PThread::start();
   }
   catch (...)
   {
      state.set(State_STOPPED);
      throw;
   }
}

void TimerWorker::stop()
{
   if (state.read() == State_STOPPED)
      return;

   selfTerminate();
   join();
   state.set(State_STOPPED);
}

void TimerWorker::run()
{
   static const unsigned suicideTimeoutMs = 10000;

   while (!getSelfTerminate())
   {
      // try twice to dequeue an item - once not waiting at all, once waiting the full idle timeout.
      // otherwise, an empty queue will cause worker threads to die immediatly, which is not what
      // we want.
      auto item = workList->deque(0);
      if (!item.first)
         item = workList->deque(suicideTimeoutMs);

      if (!item.first && !keepAlive)
         break;

      if (item.first && item.second)
         item.second();
   }

   state.set(State_STOPPING);
   queue->deactivateWorker(this);
}



TimerQueue::TimerQueue(unsigned minPoolSize, unsigned maxPoolSize)
   : PThread("TimerQ"), entrySequence(0)
{
   for (unsigned i = 0; i < maxPoolSize; i++)
   {
      workerPool[i].reset(new TimerWorker(*this, workList, i, i < minPoolSize));
      inactiveWorkers[i] = workerPool[i].get();
   }
}

TimerQueue::~TimerQueue()
{
   if (getID())
   {
      selfTerminate();
      condition.signal();
      join();
   }

   for (auto it = workerPool.begin(); it != workerPool.end(); ++it)
   {
      if (!it->second->isRunning())
         continue;

      it->second->selfTerminate();
   }

   for (auto it = workerPool.begin(); it != workerPool.end(); ++it)
      workList.enqueue({});

   for (auto it = workerPool.begin(); it != workerPool.end(); ++it)
      it->second->stop();
}

/**
 * Enqueues a function for execution at some point in the future. All enqueued items are
 * executed by the same thread; to avoid one item blocking other items, each item should
 * execute for as little time as possible.
 *
 * @timeout timeout (in milliseconds) after which to execute the function
 * @action function to execute
 * @returns handle that can be passed to cancel
 */
TimerQueue::EntryHandle TimerQueue::enqueue(std::chrono::milliseconds timeout,
      std::function<void ()> action)
{
   std::lock_guard<Mutex> lock(mutex);

   std::pair<TimePoint, uint64_t> key{Clock::now() + timeout, entrySequence};

   entrySequence += 1;
   queue.insert({key, std::move(action)});
   condition.signal();
   return {*this, key};
}

/**
 * Cancels a deferred function. Has no effect if the function has already started executing.
 *
 * @handle handle to deferred function, must be acquired by enqueue
 */
void TimerQueue::cancel(const EntryHandle& handle)
{
   std::lock_guard<Mutex> lock(mutex);

   // assert(handle.queue == &queue)
   // assert(queue.count(handle.key))
   queue.erase(handle.key);
}

void TimerQueue::run()
{
   using namespace std::chrono;

   std::unique_lock<Mutex> lock(mutex);

   static const int idleTimeout = 60 * 1000;
   static const int workerRetryTimeout = 10;
   static const int activeTimeout = 0;

   int waitTimeout = idleTimeout;

   while (!getSelfTerminate())
   {
      // wait one minute if nothing is queued, otherwise wait for at most one minute
      if (queue.empty())
      {
         condition.timedwait(&mutex, waitTimeout);
      }
      else
      {
         auto timeToFirstAction = queue.begin()->first.first - Clock::now();

         // round up to next millisecond, based on the finest specified duration type
         if (duration_cast<nanoseconds>(timeToFirstAction).count() % 1000000 > 0)
            timeToFirstAction += milliseconds(1);

         if (timeToFirstAction.count() > 0)
            condition.timedwait(
                  &mutex,
                  std::min<Clock::rep>(
                     duration_cast<milliseconds>(timeToFirstAction).count(),
                     waitTimeout));
      }

      waitTimeout = idleTimeout;

      // if we have queued items, we can be in one of two situations:
      //  * we have scheduled the item a short time ago, but no worker thread has dequeued it yet
      //  * we have scheduled the item a waitTimeout time ago, but no worker has dequeued it yet
      //
      // in both cases, we will want to wake up (hopefully) enough workers to process the items
      // that are currently waiting, since we will only add more to the queue.
      if (workList.waitingItems() > 0)
      {
         if (!requestWorkers(workList.waitingItems()))
            waitTimeout = workerRetryTimeout;
      }

      if (queue.empty())
         continue;

      bool processedAny = false;

      // do not process more items than we had in the queue to begin with. if we do not limit this,
      // an item that requeues itself with delay 0 would never allow the thread to terminate.
      for (auto size = queue.size(); size > 0; size--)
      {
         auto earliestQueued = queue.begin();
         if (earliestQueued->first.first > Clock::now())
            break;

         auto action = std::move(earliestQueued->second);
         queue.erase(earliestQueued);

         workList.enqueue(std::move(action));
         processedAny = true;
      }

      if (processedAny)
         waitTimeout = activeTimeout;
   }
}

bool TimerQueue::requestWorkers(unsigned count)
{
   while (count > 0 && !inactiveWorkers.empty())
   {
      TimerWorker* worker = inactiveWorkers.begin()->second;

      try
      {
         worker->start();
      }
      catch (...)
      {
         return false;
      }

      count -= 1;
      inactiveWorkers.erase(inactiveWorkers.begin());
   }

   return true;
}

void TimerQueue::deactivateWorker(TimerWorker* worker)
{
   std::unique_lock<Mutex> lock(mutex);

   inactiveWorkers[worker->getID()] = worker;
}
