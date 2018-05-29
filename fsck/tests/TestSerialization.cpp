#include <common/net/message/nodes/HeartbeatRequestMsg.h>
#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/toolkit/ZipIterator.h>
#include <net/message/NetMessageFactory.h>
#include <toolkit/DatabaseTk.h>

#include <gtest/gtest.h>

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

TEST(Serialization, fsckDirEntrySerialization)
{
   FsckDirEntry dirEntryIn = DatabaseTk::createDummyFsckDirEntry();
   testObjectRoundTrip(dirEntryIn);
}

TEST(Serialization, fsckDirInodeSerialization)
{
   FsckDirInode dirInodeIn = DatabaseTk::createDummyFsckDirInode();
   testObjectRoundTrip(dirInodeIn);
}

TEST(Serialization, fsckFileInodeSerialization)
{
   FsckFileInode fileInodeIn = DatabaseTk::createDummyFsckFileInode();
   testObjectRoundTrip(fileInodeIn);
}

TEST(Serialization, fsckChunkSerialization)
{
   FsckChunk chunkIn = DatabaseTk::createDummyFsckChunk();
   testObjectRoundTrip(chunkIn);
}

TEST(Serialization, fsckContDirSerialization)
{
   FsckContDir contDirIn = DatabaseTk::createDummyFsckContDir();
   testObjectRoundTrip(contDirIn);
}

TEST(Serialization, fsckFsIDSerialization)
{
   FsckFsID fsIDIn = DatabaseTk::createDummyFsckFsID();
   testObjectRoundTrip(fsIDIn);
}
