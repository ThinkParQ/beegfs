#include "TestSerialization.h"

#include <common/net/message/nodes/HeartbeatRequestMsg.h>
#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/toolkit/ZipIterator.h>
#include <net/message/NetMessageFactory.h>
#include <toolkit/DatabaseTk.h>

void TestSerialization::testFsckDirEntrySerialization()
{
   FsckDirEntry dirEntryIn = DatabaseTk::createDummyFsckDirEntry();
   testObjectRoundTrip(dirEntryIn);
}

void TestSerialization::testFsckDirInodeSerialization()
{
   FsckDirInode dirInodeIn = DatabaseTk::createDummyFsckDirInode();
   testObjectRoundTrip(dirInodeIn);
}

void TestSerialization::testFsckFileInodeSerialization()
{
   FsckFileInode fileInodeIn = DatabaseTk::createDummyFsckFileInode();
   testObjectRoundTrip(fileInodeIn);
}

void TestSerialization::testFsckChunkSerialization()
{
   FsckChunk chunkIn = DatabaseTk::createDummyFsckChunk();
   testObjectRoundTrip(chunkIn);
}

void TestSerialization::testFsckContDirSerialization()
{
   FsckContDir contDirIn = DatabaseTk::createDummyFsckContDir();
   testObjectRoundTrip(contDirIn);
}

void TestSerialization::testFsckFsIDSerialization()
{
   FsckFsID fsIDIn = DatabaseTk::createDummyFsckFsID();
   testObjectRoundTrip(fsIDIn);
}
