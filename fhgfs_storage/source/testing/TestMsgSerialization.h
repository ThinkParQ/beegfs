#ifndef TESTMSGSERIALIZATION_H_
#define TESTMSGSERIALIZATION_H_

#include <common/net/message/NetMessage.h>
#include <common/testing/TestMsgSerializationBase.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TestMsgSerialization: public TestMsgSerializationBase
{
   CPPUNIT_TEST_SUITE( TestMsgSerialization );
   CPPUNIT_TEST_SUITE_END();

   public:
      TestMsgSerialization();
      virtual ~TestMsgSerialization();

      void setUp();
      void tearDown();
};

#endif /* TESTMSGSERIALIZATION_H_ */
