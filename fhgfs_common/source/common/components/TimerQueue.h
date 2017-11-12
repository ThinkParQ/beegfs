#ifndef COMPONENTS_TIMERQUEUE_H
#define COMPONENTS_TIMERQUEUE_H

#include <common/threading/Condition.h>
#include <common/threading/PThread.h>

#include <chrono>
#include <deque>
#include <functional>
#include <map>

class TimerQueue;

class TimerWorkList
{
   public:
      void enqueue(std::function<void ()> fn);

      std::pair<bool, std::function<void ()>> deque(int timeoutMS);

      size_t waitingItems() const;

   private:
      mutable Mutex mutex;
      Condition condition;

      std::deque<std::function<void ()>> available;
};

class TimerWorker : public PThread
{
   public:
      TimerWorker(TimerQueue& queue, TimerWorkList& workList, unsigned id, bool keepAlive);
      ~TimerWorker();

      void run() override;

      void start();
      void stop();

      bool isRunning() const
      {
         return state.read() == State_RUNNING;
      }

      unsigned getID() const { return id; }

   private:
      enum State {
         State_STOPPED,
         State_RUNNING,
         State_STOPPING,
      };

      TimerQueue* queue;
      TimerWorkList* workList;
      bool keepAlive;
      unsigned id;

      AtomicSizeT state; // holds values from the enum State
};

class TimerQueue : public PThread
{
   friend class TimerWorker;

   private:
      typedef std::chrono::steady_clock Clock;

      typedef std::chrono::time_point<Clock> TimePoint;
      typedef std::map<std::pair<TimePoint, uint64_t>, std::function<void ()>> QueueType;

      // if the system's steady_clock is not at least 1ms accurate, bail now.
      static_assert(
            std::ratio_less_equal<Clock::period, std::milli>::value,
            "System clock too coarse");

   public:
      class EntryHandle
      {
         friend class TimerQueue;

         public:
            void cancel()
            {
               queue->cancel(*this);
            }

         private:
            EntryHandle(TimerQueue& queue, std::pair<TimePoint, uint64_t> key)
               : queue(&queue), key(key)
            {}

            TimerQueue* queue;
            std::pair<TimePoint, uint64_t> key;
      };

      TimerQueue(unsigned minPoolSize = 1, unsigned maxPoolSize = 100);
      ~TimerQueue();

      // class is neither copyable nor movable for now, since we do not need that (yet)
      TimerQueue(TimerQueue&&) = delete;
      TimerQueue& operator=(TimerQueue&&) = delete;

      EntryHandle enqueue(std::chrono::milliseconds timeout, std::function<void ()> action);

      void cancel(const EntryHandle& handle);

      void run() override;

   private:
      Mutex mutex;
      Condition condition;

      uint64_t entrySequence;

      QueueType queue;

      TimerWorkList workList;

      // these have to be pointers instead of values because older libstdc++ versions do not have
      // map::emplace.
      std::map<unsigned, std::shared_ptr<TimerWorker>> workerPool;
      std::map<unsigned, TimerWorker*> inactiveWorkers;

      bool requestWorkers(unsigned count);

      void deactivateWorker(TimerWorker* worker);
};

#endif
