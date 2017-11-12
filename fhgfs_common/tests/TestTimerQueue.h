#ifndef TESTTIMERQUEUE_H_
#define TESTTIMERQUEUE_H_

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <common/components/TimerQueue.h>

class TestTimerQueue : public CppUnit::TestFixture
{
   CPPUNIT_TEST_SUITE(TestTimerQueue);

   CPPUNIT_TEST(enqueueCancel);
   CPPUNIT_TEST(enqueueManyLong);

   CPPUNIT_TEST_SUITE_END();

   public:
      void setUp();
      void tearDown();

   private:
      std::unique_ptr<TimerQueue> queue;

      void enqueueCancel();
      void enqueueManyLong();
};

#endif
