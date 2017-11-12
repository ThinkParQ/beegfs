#include "TestTimerQueue.h"

#include <common/threading/Barrier.h>
#include <tests/TestRunnerBase.h>

REGISTER_TEST_SUITE(TestTimerQueue);

void TestTimerQueue::setUp()
{
   queue.reset(new TimerQueue(0, 20));
}

void TestTimerQueue::tearDown()
{
   queue.reset();
}

namespace {
   struct EnqueueCancelFn
   {
      AtomicSizeT* count;

      void operator()()
      {
         count->increase();
      }
   };
}

void TestTimerQueue::enqueueCancel()
{
   AtomicSizeT count(0);

   auto handle = queue->enqueue(std::chrono::milliseconds(100), EnqueueCancelFn{&count});
   handle.cancel();

   sleep(1);
   CPPUNIT_ASSERT(count.read() == 0);
}

namespace {
   struct EnqueueManyLongFn
   {
      void operator()(AtomicSizeT& count)
      {
         sleep(1);
         count.increase();
      }
   };
}

void TestTimerQueue::enqueueManyLong()
{
   AtomicSizeT count(0);
   EnqueueManyLongFn fn;

   for (int i = 0; i < 42; i++)
      queue->enqueue(
            std::chrono::milliseconds(10),
            std::bind(&EnqueueManyLongFn::operator(), &fn, std::ref(count)));

   Time begin;
   while (count.read() != 42)
      sleep(1);

   Time end;

   CPPUNIT_ASSERT(end.elapsedSinceMS(&begin) < 20 * 1000);
}
