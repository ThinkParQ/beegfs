#ifndef TESTMSGSERIALIZATION_H_
#define TESTMSGSERIALIZATION_H_

#include <common/app/log/LogContext.h>
#include <common/net/message/NetMessage.h>
#include <common/testing/TestMsgSerializationBase.h>
#include <cppunit/TestAssert.h>
#include <cppunit/extensions/HelperMacros.h>

/*
 * This class is intended to hold all tests to check the serialization/deserialization of messages,
 * which are sent in this project. Please note, that each message tested here must implement
 * the method "testingEquals".
 */
class TestMsgSerialization: public TestMsgSerializationBase
{
   CPPUNIT_TEST_SUITE( TestMsgSerialization );
   CPPUNIT_TEST( testMkDirMsgSerialization );
   CPPUNIT_TEST( testFsckModificationEventMsgSerialization );
   CPPUNIT_TEST( testCloseChunkFileMsgSerialization );
   CPPUNIT_TEST( testHardlinkMsgSerialization );
   CPPUNIT_TEST_SUITE_END();

   public:
      TestMsgSerialization();
      virtual ~TestMsgSerialization();

      void setUp();
      void tearDown();

      void testMkDirMsgSerialization();
      void testFsckModificationEventMsgSerialization();
      void testCloseChunkFileMsgSerialization();
      void testHardlinkMsgSerialization();

   private:
      LogContext log;
};

#endif /* TESTMETASERIALIZATION_H_ */
