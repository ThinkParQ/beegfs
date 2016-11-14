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



TimerWorker::TimerWorker(TimerWorkList& workList, unsigned id, bool keepAlive):
   PThread("TimerWork/" + StringTk::uintToStr(id)), workList(&workList),
   keepAlive(keepAlive), state(State_STOPPED)
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
}



TimerQueue::TimerQueue(unsigned minPoolSize, unsigned maxPoolSize)
   : PThread("TimerQ"), entrySequence(0)
{
   for (unsigned i = 0; i < maxPoolSize; i++)
      workerPool[i].reset(new TimerWorker(workList, i, i < minPoolSize));

   start();
}

TimerQueue::~TimerQueue()
{
   selfTerminate();
   {
      std::unique_lock<Mutex> lock(mutex);
      condition.signal();
   }
   join();

   for (auto it = workerPool.begin(); it != workerPool.end(); ++it)
   {
      if (!it->second->isRunning())
         continue;

      it->second->selfTerminate();
   }

   for (auto it = workerPool.begin(); it != workerPool.end(); ++it)
      workList.enqueue({});
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
   std::unique_lock<Mutex> lock(mutex);

   static const int idleTimeout = 1000;
   static const int activeTimeout = 100;

   int waitTimeout = activeTimeout;

   while (!getSelfTerminate())
   {
      // wait one second if nothing is queued, otherwise wait for at most one second
      if (queue.empty())
      {
         condition.timedwait(&mutex, waitTimeout);
      }
      else
      {
         auto timeToFirstAction = queue.begin()->first.first - Clock::now();
         if (timeToFirstAction.count() > 0)
            condition.timedwait(
                  &mutex,
                  std::min<Clock::rep>(
                     std::chrono::duration_cast<std::chrono::milliseconds>(timeToFirstAction)
                        .count(),
                     waitTimeout));
      }

      // if we have queued items, we can be in one of two situations:
      //  * we have scheduled the item a short time ago, but no worker thread has dequeued it yet
      //  * we have scheduled the item a waitTimeout time ago, but no worker has dequeued it yet
      //
      // in both cases, we will want to wake up (hopefully) enough workers to process the items
      // that are currently waiting, since we will only add more to the queue.
      if (workList.waitingItems() > 0)
         requestWorkers(workList.waitingItems());

      if (queue.empty())
      {
         waitTimeout = idleTimeout;
         continue;
      }

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
      }

      waitTimeout = activeTimeout;
   }
}

void TimerQueue::requestWorkers(unsigned count)
{
   for (auto it = workerPool.begin(); count > 0 && it != workerPool.end(); ++it)
   {
      if (it->second->isRunning())
         continue;

      it->second->start();
      count -= 1;
   }
}
