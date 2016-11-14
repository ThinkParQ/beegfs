#ifndef TESTSERIALIZATION_H_
#define TESTSERIALIZATION_H_

#include <common/app/log/LogContext.h>
#include <common/net/message/NetMessage.h>
#include <common/testing/TestMsgSerializationBase.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <session/SessionStore.h>

/*
 * intended to test all kind of serialization/deserialization of objects (not for messages, only
 * for the single objects itself
 */
class TestSerialization: public CppUnit::TestFixture
{
   CPPUNIT_TEST_SUITE(TestSerialization);
   CPPUNIT_TEST(testSessionSerialization);
   CPPUNIT_TEST(testDynamicFileAttribs);
   CPPUNIT_TEST(testChunkFileInfo);
   CPPUNIT_TEST(testEntryLockDetails);
   CPPUNIT_TEST(testRangeLockDetails);
   CPPUNIT_TEST_SUITE_END();

   public:
      TestSerialization();

      void testSessionSerialization();
      void testDynamicFileAttribs();
      void testChunkFileInfo();
      void testEntryLockDetails();
      void testRangeLockDetails();

   private:
      LogContext log;

      void initSessionStoreForTests(SessionStore& testSessionStore);
      static void addRandomValuesToUInt16Vector(UInt16Vector* vector, size_t count);
      static void addRandomTargetChunkBlocksToStatData(StatData* statData, size_t count);

      template<typename Obj>
      static void testObjectRoundTrip(Obj& data)
      {
         Serializer ser;
         ser % data;

         std::vector<char> buffer(ser.size());

         ser = Serializer(&buffer[0], buffer.size());
         ser % data;
         CPPUNIT_ASSERT(ser.good());
         CPPUNIT_ASSERT(ser.size() == buffer.size());

         Deserializer des(&buffer[0], buffer.size());

         Obj read;

         des % read;
         CPPUNIT_ASSERT(des.good());
         CPPUNIT_ASSERT(des.size() == buffer.size());

         ser = Serializer();
         ser % read;
         CPPUNIT_ASSERT(ser.size() == buffer.size());

         std::vector<char> rtBuffer(buffer.size());

         ser = Serializer(&rtBuffer[0], rtBuffer.size());
         ser % read;
         CPPUNIT_ASSERT(ser.good());
         CPPUNIT_ASSERT(ser.size() == rtBuffer.size());
         CPPUNIT_ASSERT(buffer == rtBuffer);
      }
};

#endif /* TESTSERIALIZATION_H_ */
