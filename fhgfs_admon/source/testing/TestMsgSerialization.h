#ifndef TESTMSGSERIALIZATION_H_
#define TESTMSGSERIALIZATION_H_

#include <common/app/log/LogContext.h>
#include <common/net/message/NetMessage.h>
#include <common/testing/TestMsgSerializationBase.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TestMsgSerialization: public TestMsgSerializationBase
{
   CPPUNIT_TEST_SUITE( TestMsgSerialization );
   CPPUNIT_TEST(testGetNodeInfoMsgSerialization);
   CPPUNIT_TEST(testGetNodeInfoRespMsgSerialization);
   CPPUNIT_TEST_SUITE_END();

   public:
      TestMsgSerialization();
      virtual ~TestMsgSerialization();

      void testGetNodeInfoMsgSerialization();
      void testGetNodeInfoRespMsgSerialization();

      void setUp();
      void tearDown();

   private:
      LogContext log;
};

#endif /* TESTMSGSERIALIZATION_H_ */
