#ifndef TESTMSGSERIALIZATIONBASE_H_
#define TESTMSGSERIALIZATIONBASE_H_

#include <common/net/message/NetMessage.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TestMsgSerializationBase: public CppUnit::TestFixture
{
   public:
      TestMsgSerializationBase();
      virtual ~TestMsgSerializationBase();

      void setUp();
      void tearDown();

   protected:
      bool testMsgSerialization(NetMessage &msg, NetMessage &msgClone);

      template<typename T>
      bool testMsgSerialization(T msg)
      {
         T msgClone;

         return testMsgSerialization(msg, msgClone);
      }
};

#endif /* TESTMSGSERIALIZATIONBASE_H_ */
