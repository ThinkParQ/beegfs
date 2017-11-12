#ifndef TESTSERIALIZATION_H_
#define TESTSERIALIZATION_H_

#include <common/net/message/NetMessage.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TestSerialization: public CppUnit::TestFixture
{
   CPPUNIT_TEST_SUITE( TestSerialization );

   CPPUNIT_TEST(testFsckDirEntrySerialization);
   CPPUNIT_TEST(testFsckDirInodeSerialization);
   CPPUNIT_TEST(testFsckFileInodeSerialization);
   CPPUNIT_TEST(testFsckChunkSerialization);
   CPPUNIT_TEST(testFsckContDirSerialization);
   CPPUNIT_TEST(testFsckFsIDSerialization);
   CPPUNIT_TEST_SUITE_END();

   private:
      void testFsckDirEntrySerialization();
      void testFsckDirInodeSerialization();
      void testFsckFileInodeSerialization();
      void testFsckChunkSerialization();
      void testFsckContDirSerialization();
      void testFsckFsIDSerialization();

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
