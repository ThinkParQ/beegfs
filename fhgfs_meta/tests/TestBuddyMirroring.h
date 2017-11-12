#ifndef TESTBUDDYMIRRORING_H
#define TESTBUDDYMIRRORING_H

#include <common/app/log/LogContext.h>
#include <session/EntryLockStore.h>

#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TestBuddyMirroring: public CppUnit::TestFixture
{
   CPPUNIT_TEST_SUITE( TestBuddyMirroring );
   CPPUNIT_TEST( testSimpleEntryLocks );
   CPPUNIT_TEST( testRWEntryLocks );
   CPPUNIT_TEST_SUITE_END();

   public:
      TestBuddyMirroring();
      virtual ~TestBuddyMirroring();

      void setUp();
      void tearDown();

      void testSimpleEntryLocks();
      void testRWEntryLocks();

   private:
      LogContext log;
      EntryLockStore entryLockStore;
};

#endif /* TESTBUDDYMIRRORING_H */
