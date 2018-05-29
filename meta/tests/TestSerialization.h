#ifndef TESTSERIALIZATION_H_
#define TESTSERIALIZATION_H_

#include <common/net/message/NetMessage.h>
#include <gtest/gtest.h>
#include <session/SessionStore.h>

/*
 * intended to test all kind of serialization/deserialization of objects (not for messages, only
 * for the single objects itself
 */
class TestSerialization: public ::testing::Test
{
   protected:
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
         ASSERT_TRUE(ser.good());
         ASSERT_EQ(ser.size(), buffer.size());

         Deserializer des(&buffer[0], buffer.size());

         Obj read;

         des % read;
         ASSERT_TRUE(des.good());
         ASSERT_EQ(des.size(), buffer.size());

         ser = Serializer();
         ser % read;
         ASSERT_EQ(ser.size(), buffer.size());

         std::vector<char> rtBuffer(buffer.size());

         ser = Serializer(&rtBuffer[0], rtBuffer.size());
         ser % read;
         ASSERT_TRUE(ser.good());
         ASSERT_EQ(ser.size(), rtBuffer.size());
         ASSERT_EQ(buffer, rtBuffer);
      }
};

#endif /* TESTSERIALIZATION_H_ */
