#include <common/components/TimerQueue.h>
#include <common/threading/Barrier.h>

#include <gtest/gtest.h>

class TestTimerQueue : public ::testing::Test {
   protected:
      std::unique_ptr<TimerQueue> queue;

      void SetUp() override
      {
         queue.reset(new TimerQueue(0, 20));
         queue->start();
      }
};

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

TEST_F(TestTimerQueue, enqueueCancel)
{
   AtomicSizeT count(0);

   auto handle = queue->enqueue(std::chrono::milliseconds(100), EnqueueCancelFn{&count});
   handle.cancel();

   sleep(1);
   ASSERT_EQ(count.read(), 0u);
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

TEST_F(TestTimerQueue, enqueueManyLong)
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

   ASSERT_LT(end.elapsedSinceMS(&begin), 20 * 1000u);
}
