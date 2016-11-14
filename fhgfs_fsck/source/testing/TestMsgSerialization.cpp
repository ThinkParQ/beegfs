#include "TestMsgSerialization.h"

#include <common/net/message/fsck/AdjustChunkPermissionsMsg.h>
#include <common/net/message/fsck/AdjustChunkPermissionsRespMsg.h>
#include <common/net/message/fsck/CreateDefDirInodesMsg.h>
#include <common/net/message/fsck/CreateDefDirInodesRespMsg.h>
#include <common/net/message/fsck/CreateEmptyContDirsMsg.h>
#include <common/net/message/fsck/CreateEmptyContDirsRespMsg.h>
#include <common/net/message/fsck/DeleteChunksMsg.h>
#include <common/net/message/fsck/DeleteChunksRespMsg.h>
#include <common/net/message/fsck/DeleteDirEntriesMsg.h>
#include <common/net/message/fsck/DeleteDirEntriesRespMsg.h>
#include <common/net/message/fsck/FetchFsckChunkListMsg.h>
#include <common/net/message/fsck/FetchFsckChunkListRespMsg.h>
#include <common/net/message/fsck/FixInodeOwnersInDentryMsg.h>
#include <common/net/message/fsck/FixInodeOwnersInDentryRespMsg.h>
#include <common/net/message/fsck/FixInodeOwnersMsg.h>
#include <common/net/message/fsck/FixInodeOwnersRespMsg.h>
#include <common/net/message/fsck/FsckSetEventLoggingMsg.h>
#include <common/net/message/fsck/FsckSetEventLoggingRespMsg.h>
#include <common/net/message/fsck/LinkToLostAndFoundMsg.h>
#include <common/net/message/fsck/LinkToLostAndFoundRespMsg.h>
#include <common/net/message/fsck/MoveChunkFileMsg.h>
#include <common/net/message/fsck/MoveChunkFileRespMsg.h>
#include <common/net/message/fsck/RecreateDentriesMsg.h>
#include <common/net/message/fsck/RecreateDentriesRespMsg.h>
#include <common/net/message/fsck/RecreateFsIDsMsg.h>
#include <common/net/message/fsck/RecreateFsIDsRespMsg.h>
#include <common/net/message/fsck/RemoveInodesMsg.h>
#include <common/net/message/fsck/RemoveInodesRespMsg.h>
#include <common/net/message/fsck/RetrieveDirEntriesMsg.h>
#include <common/net/message/fsck/RetrieveDirEntriesRespMsg.h>
#include <common/net/message/fsck/RetrieveFsIDsMsg.h>
#include <common/net/message/fsck/RetrieveFsIDsRespMsg.h>
#include <common/net/message/fsck/RetrieveInodesMsg.h>
#include <common/net/message/fsck/RetrieveInodesRespMsg.h>
#include <common/net/message/fsck/UpdateDirAttribsMsg.h>
#include <common/net/message/fsck/UpdateDirAttribsRespMsg.h>
#include <common/net/message/fsck/UpdateFileAttribsMsg.h>
#include <common/net/message/fsck/UpdateFileAttribsRespMsg.h>
#include <toolkit/DatabaseTk.h>


TestMsgSerialization::TestMsgSerialization()
   : log("TestMsgSerialization")
{
}

void TestMsgSerialization::testRetrieveDirEntriesMsgSerialization()
{
   log.log(Log_DEBUG, "testRetrieveDirEntriesMsgSerialization started");

   unsigned hashDirNum = 55;
   std::string currentContDirID = "currentContDir";
   unsigned maxOutEntries = 150;
   int64_t lastHashDirOffset = 123456789;
   int64_t lastContDirOffset = 987654321;

   RetrieveDirEntriesMsg msg(hashDirNum, currentContDirID, maxOutEntries, lastHashDirOffset,
      lastContDirOffset, true);
   RetrieveDirEntriesMsg msgClone;

   bool testRes = this->testMsgSerialization(msg, msgClone);

   if (!testRes)
      CPPUNIT_FAIL("Failed to serialize/deserialize RetrieveDirEntriesMsg");

   log.log(Log_DEBUG, "testRetrieveDirEntriesMsgSerialization finished");
}

void TestMsgSerialization::testRetrieveDirEntriesRespMsgSerialization()
{
   log.log(Log_DEBUG, "testRetrieveDirEntriesRespMsgSerialization started");

   FsckContDirList fsckContDirs;
   DatabaseTk::createDummyFsckContDirs(100, &fsckContDirs);

   FsckDirEntryList fsckDirEntries;
   DatabaseTk::createDummyFsckDirEntries(100, &fsckDirEntries);

   FsckFileInodeList inlinedFileInodes;
   DatabaseTk::createDummyFsckFileInodes(100, &inlinedFileInodes);

   std::string currentContDirID = "currentContDir";
   int64_t newHashDirOffset = 123456789;
   int64_t newContDirOffset = 987654321;

   RetrieveDirEntriesRespMsg msg(&fsckContDirs, &fsckDirEntries, &inlinedFileInodes,
      currentContDirID, newHashDirOffset, newContDirOffset);
   RetrieveDirEntriesRespMsg msgClone;

   bool testRes = this->testMsgSerialization(msg, msgClone);

   if (!testRes)
      CPPUNIT_FAIL("Failed to serialize/deserialize RetrieveDirEntriesRespMsg");

   log.log(Log_DEBUG, "testRetrieveDirEntriesRespMsgSerialization finished");
}

void TestMsgSerialization::testRetrieveFsIDsMsgSerialization()
{
   log.log(Log_DEBUG, "testRetrieveFsIDsMsgSerialization started");

   unsigned hashDirNum = 55;
   std::string currentContDirID = "currentContDir";
   unsigned maxOutEntries = 150;
   int64_t lastHashDirOffset = 123456789;
   int64_t lastContDirOffset = 987654321;

   RetrieveFsIDsMsg msg(hashDirNum, false, currentContDirID, maxOutEntries, lastHashDirOffset,
      lastContDirOffset);
   RetrieveFsIDsMsg msgClone;

   bool testRes = this->testMsgSerialization(msg, msgClone);

   if (!testRes)
      CPPUNIT_FAIL("Failed to serialize/deserialize RetrieveFsIDsMsg");

   log.log(Log_DEBUG, "testRetrieveFsIDsMsgSerialization finished");
}

void TestMsgSerialization::testRetrieveFsIDsRespMsgSerialization()
{
   log.log(Log_DEBUG, "testRetrieveFsIDsRespMsgSerialization started");

   FsckFsIDList fsIDs;
   DatabaseTk::createDummyFsckFsIDs(100, &fsIDs);

   std::string currentContDirID = "currentContDir";
   int64_t newHashDirOffset = 123456789;
   int64_t newContDirOffset = 987654321;

   RetrieveFsIDsRespMsg msg(&fsIDs, currentContDirID, newHashDirOffset, newContDirOffset);
   RetrieveFsIDsRespMsg msgClone;

   bool testRes = this->testMsgSerialization(msg, msgClone);

   if (!testRes)
      CPPUNIT_FAIL("Failed to serialize/deserialize RetrieveFsIDsRespMsg");

   log.log(Log_DEBUG, "testRetrieveFsIDsRespMsgSerialization finished");
}

void TestMsgSerialization::testRetrieveInodesMsgSerialization()
{
   log.log(Log_DEBUG, "testRetrieveInodesMsgSerialization started");

   unsigned hashDirNum = 55;
   unsigned maxOutInodes = 150;
   int64_t lastOffset = 123456789;

   RetrieveInodesMsg msg(hashDirNum, lastOffset, maxOutInodes, true);
   RetrieveInodesMsg msgClone;

   bool testRes = this->testMsgSerialization(msg, msgClone);

   if (!testRes)
      CPPUNIT_FAIL("Failed to serialize/deserialize RetrieveInodesMsg");

   log.log(Log_DEBUG, "testRetrieveInodesMsgSerialization finished");
}

void TestMsgSerialization::testRetrieveInodesRespMsgSerialization()
{
   log.log(Log_DEBUG, "testRetrieveInodesRespMsgSerialization started");

   FsckFileInodeList fileInodes;
   DatabaseTk::createDummyFsckFileInodes(100, &fileInodes);

   FsckDirInodeList dirInodes;
   DatabaseTk::createDummyFsckDirInodes(100, &dirInodes);

   int64_t lastOffset = 123456789;

   RetrieveInodesRespMsg msg(&fileInodes, &dirInodes, lastOffset);
   RetrieveInodesRespMsg msgClone;

   bool testRes = this->testMsgSerialization(msg, msgClone);

   if (!testRes)
      CPPUNIT_FAIL("Failed to serialize/deserialize RetrieveInodesRespMsg");

   log.log(Log_DEBUG, "testRetrieveInodesRespMsgSerialization finished");
}

void TestMsgSerialization::testFetchFsckChunkListMsgSerialization()
{
   log.log(Log_DEBUG, "testFetchFsckChunkListMsgSerialization started");

   FsckChunkList chunkList;
   DatabaseTk::createDummyFsckChunks(100, &chunkList);

   unsigned maxNum = 100;
   FetchFsckChunkListStatus lastStatus = (FetchFsckChunkListStatus)3;
   bool forceRestart = true;

   FetchFsckChunkListMsg msg(maxNum, lastStatus, forceRestart);
   FetchFsckChunkListMsg msgClone;

   bool testRes = this->testMsgSerialization(msg, msgClone);

   if (!testRes)
      CPPUNIT_FAIL("Failed to serialize/deserialize FetchFsckChunkListMsg");

   log.log(Log_DEBUG, "testFetchFsckChunkListMsgSerialization finished");
}

void TestMsgSerialization::testFetchFsckChunkListRespMsgSerialization()
{
   log.log(Log_DEBUG, "testFetchFsckChunkListRespMsgSerialization started");

   FsckChunkList chunkList;
   DatabaseTk::createDummyFsckChunks(100, &chunkList);

   FetchFsckChunkListStatus status = (FetchFsckChunkListStatus)3;

   FetchFsckChunkListRespMsg msg(&chunkList, status);
   FetchFsckChunkListRespMsg msgClone;

   bool testRes = this->testMsgSerialization(msg, msgClone);

   if (!testRes)
      CPPUNIT_FAIL("Failed to serialize/deserialize FetchFsckChunkListRespMsg");

   log.log(Log_DEBUG, "testFetchFsckChunkListRespMsgSerialization finished");
}

void TestMsgSerialization::testFsckSetEventLoggingMsg()
{
   log.log(Log_DEBUG, "testFsckSetEventLoggingMsg started");

   NicAddressList nicList;
   FsckSetEventLoggingMsg msg(true, 1024, &nicList, false);

   FsckSetEventLoggingMsg msgClone;

   bool testRes = this->testMsgSerialization(msg, msgClone);

   if (!testRes)
      CPPUNIT_FAIL("Failed to serialize/deserialize FsckSetEventLoggingMsg");

   log.log(Log_DEBUG, "testFsckSetEventLoggingMsg finished");
}

void TestMsgSerialization::testFsckSetEventLoggingRespMsg()
{
   log.log(Log_DEBUG, "testFsckSetEventLoggingRespMsg started");

   FsckSetEventLoggingRespMsg msg(true, false, true);

   FsckSetEventLoggingRespMsg msgClone;

   bool testRes = this->testMsgSerialization(msg, msgClone);

   if (!testRes)
      CPPUNIT_FAIL("Failed to serialize/deserialize FsckSetEventLoggingRespMsg");

   log.log(Log_DEBUG, "testFsckSetEventLoggingRespMsg finished");
}

void TestMsgSerialization::testAdjustChunkPermissionsMsgSerialization()
{
   log.log(Log_DEBUG, "testAdjustChunkPermissionsMsgSerialization started");

   std::string currentContDirID = "currentContDirID";
   AdjustChunkPermissionsMsg msg(1000, currentContDirID, 2000, 3000, 4000, true);

   AdjustChunkPermissionsMsg msgClone;

   bool testRes = this->testMsgSerialization(msg, msgClone);

   if (!testRes)
      CPPUNIT_FAIL("Failed to serialize/deserialize AdjustChunkPermissionsMsg");

   log.log(Log_DEBUG, "testAdjustChunkPermissionsMsgSerialization finished");
}

void TestMsgSerialization::testAdjustChunkPermissionsRespMsgSerialization()
{
   log.log(Log_DEBUG, "testAdjustChunkPermissionsRespMsgSerialization started");

   std::string currentContDirID = "currentContDirID";
   AdjustChunkPermissionsRespMsg msg(1000, currentContDirID, 2000, 3000, true);

   AdjustChunkPermissionsRespMsg msgClone;

   bool testRes = this->testMsgSerialization(msg, msgClone);

   if (!testRes)
      CPPUNIT_FAIL("Failed to serialize/deserialize AdjustChunkPermissionsRespMsg");

   log.log(Log_DEBUG, "testAdjustChunkPermissionsRespMsgSerialization finished");
}

void TestMsgSerialization::testMoveChunkFileMsg()
{
   log.log(Log_DEBUG, "testMoveChunkFileMsg started");

   std::string chunkName = "chunkName";
   uint16_t targetID = 1000;
   uint16_t mirroredFromTargetID = 2000;
   std::string oldPath = "path/to/old/path";
   std::string newPath = "path/to/new/path";
   MoveChunkFileMsg msg(chunkName, targetID, mirroredFromTargetID, oldPath, newPath);

   MoveChunkFileMsg msgClone;

   bool testRes = this->testMsgSerialization(msg, msgClone);

   if (!testRes)
      CPPUNIT_FAIL("Failed to serialize/deserialize MoveChunkFileMsg");

   log.log(Log_DEBUG, "testMoveChunkFileMsg finished");
}

void TestMsgSerialization::testMoveChunkFileRespMsg()
{
   log.log(Log_DEBUG, "testMoveChunkFileRespMsg started");

   int result = 1000;
   MoveChunkFileRespMsg msg(result);

   MoveChunkFileRespMsg msgClone;

   bool testRes = this->testMsgSerialization(msg, msgClone);

   if (!testRes)
      CPPUNIT_FAIL("Failed to serialize/deserialize MoveChunkFileRespMsg");

   log.log(Log_DEBUG, "testMoveChunkFileRespMsg finished");
}

void TestMsgSerialization::testRemoveInodesMsg()
{
   CPPUNIT_ASSERT(
      testMsgSerialization(
         RemoveInodesMsg({
            RemoveInodesMsg::Item("1-1-1", DirEntryType_FIFO, false),
            RemoveInodesMsg::Item("", DirEntryType_DIRECTORY, true)})));
}

void TestMsgSerialization::testRemoveInodesRespMsg()
{
   CPPUNIT_ASSERT(
      testMsgSerialization(
         RemoveInodesRespMsg(StringList({"1", "2", ""}))));
}

void TestMsgSerialization::testCreateDefDirInodesMsg()
{
   CPPUNIT_ASSERT(
      testMsgSerialization(
         CreateDefDirInodesMsg({
            CreateDefDirInodesMsg::Item("1-1-1", false),
            CreateDefDirInodesMsg::Item("2-1-1", true)})));
}

void TestMsgSerialization::testCreateDefDirInodesRespMsg()
{
   FsckDirInodeList created;
   DatabaseTk::createDummyFsckDirInodes(4, &created);

   StringList failed;
   failed.push_back("11");
   failed.push_back("12");

   CreateDefDirInodesRespMsg msg(&failed, &created);
   CreateDefDirInodesRespMsg clone;

   CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
}

void TestMsgSerialization::testCreateEmptyContDirsMsg()
{
   CreateEmptyContDirsMsg msg({
         CreateEmptyContDirsMsg::Item("1", false),
         CreateEmptyContDirsMsg::Item("2", true)});
   CreateEmptyContDirsMsg clone;

   CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
}

void TestMsgSerialization::testCreateEmptyContDirsRespMsg()
{
   StringList ids;
   ids.push_back("1");
   ids.push_back("2");

   CreateEmptyContDirsRespMsg msg(&ids);
   CreateEmptyContDirsRespMsg clone;

   CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
}

void TestMsgSerialization::testDeleteChunksMsg()
{
   FsckChunkList chunks;
   DatabaseTk::createDummyFsckChunks(4, &chunks);

   DeleteChunksMsg msg(&chunks);
   DeleteChunksMsg clone;

   CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
}

void TestMsgSerialization::testDeleteChunksRespMsg()
{
   FsckChunkList chunks;
   DatabaseTk::createDummyFsckChunks(4, &chunks);

   DeleteChunksRespMsg msg(&chunks);
   DeleteChunksRespMsg clone;

   CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
}

void TestMsgSerialization::testDeleteDirEntriesMsg()
{
   FsckDirEntryList dentries;
   DatabaseTk::createDummyFsckDirEntries(4, &dentries);

   DeleteDirEntriesMsg msg(&dentries);
   DeleteDirEntriesMsg clone;

   CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
}

void TestMsgSerialization::testDeleteDirEntriesRespMsg()
{
   FsckDirEntryList dentries;
   DatabaseTk::createDummyFsckDirEntries(4, &dentries);

   DeleteDirEntriesRespMsg msg(&dentries);
   DeleteDirEntriesRespMsg clone;

   CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
}

void TestMsgSerialization::testFixInodeOwnersInDentryMsg()
{
   FsckDirEntryList dentries;
   DatabaseTk::createDummyFsckDirEntries(4, &dentries);

   NumNodeIDList owners;
   owners.push_back(NumNodeID(0));
   owners.push_back(NumNodeID(1));
   owners.push_back(NumNodeID(2));
   owners.push_back(NumNodeID(3));

   FixInodeOwnersInDentryMsg msg(dentries, owners);
   FixInodeOwnersInDentryMsg clone;

   CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
}

void TestMsgSerialization::testFixInodeOwnersInDentryRespMsg()
{
   FsckDirEntryList dentries;
   DatabaseTk::createDummyFsckDirEntries(4, &dentries);

   FixInodeOwnersInDentryRespMsg msg(&dentries);
   FixInodeOwnersInDentryRespMsg clone;

   CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
}

void TestMsgSerialization::testFixInodeOwnersMsg()
{
   FsckDirInodeList inodes;
   DatabaseTk::createDummyFsckDirInodes(4, &inodes);

   FixInodeOwnersMsg msg(&inodes);
   FixInodeOwnersMsg clone;

   CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
}

void TestMsgSerialization::testFixInodeOwnersRespMsg()
{
   FsckDirInodeList inodes;
   DatabaseTk::createDummyFsckDirInodes(4, &inodes);

   FixInodeOwnersRespMsg msg(&inodes);
   FixInodeOwnersRespMsg clone;

   CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
}

void TestMsgSerialization::testLinkToLostAndFoundMsg()
{
   FsckDirInodeList dirs;
   DatabaseTk::createDummyFsckDirInodes(4, &dirs);

   FsckFileInodeList files;
   DatabaseTk::createDummyFsckFileInodes(4, &files);

   EntryInfo laf(NumNodeID(0), "snafu", "bar", "laf", DirEntryType_DIRECTORY, 0);

   {
      LinkToLostAndFoundMsg msg(&laf, &dirs);
      LinkToLostAndFoundMsg clone;

      CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
   }

   {
      LinkToLostAndFoundMsg msg(&laf, &files);
      LinkToLostAndFoundMsg clone;

      CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
   }
}

void TestMsgSerialization::testLinkToLostAndFoundRespMsg()
{
   FsckDirInodeList dirs;
   DatabaseTk::createDummyFsckDirInodes(4, &dirs);

   FsckFileInodeList files;
   DatabaseTk::createDummyFsckFileInodes(4, &files);

   FsckDirEntryList entries;
   DatabaseTk::createDummyFsckDirEntries(4, &entries);

   {
      LinkToLostAndFoundRespMsg msg(&dirs, &entries);
      LinkToLostAndFoundRespMsg clone;

      CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
   }

   {
      LinkToLostAndFoundRespMsg msg(&files, &entries);
      LinkToLostAndFoundRespMsg clone;

      CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
   }
}

void TestMsgSerialization::testRecreateDentriesMsg()
{
   FsckFsIDList ids;
   DatabaseTk::createDummyFsckFsIDs(4, &ids);

   RecreateDentriesMsg msg(&ids);
   RecreateDentriesMsg clone;

   CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
}

void TestMsgSerialization::testRecreateDentriesRespMsg()
{
   FsckFsIDList ids;
   DatabaseTk::createDummyFsckFsIDs(4, &ids);

   FsckDirEntryList dentries;
   DatabaseTk::createDummyFsckDirEntries(4, &dentries);

   FsckFileInodeList inodes;
   DatabaseTk::createDummyFsckFileInodes(4, &inodes);

   RecreateDentriesRespMsg msg(&ids, &dentries, &inodes);
   RecreateDentriesRespMsg clone;

   CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
}

void TestMsgSerialization::testRecreateFsIDsMsg()
{
   FsckDirEntryList dentries;
   DatabaseTk::createDummyFsckDirEntries(4, &dentries);

   RecreateFsIDsMsg msg(&dentries);
   RecreateFsIDsMsg clone;

   CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
}

void TestMsgSerialization::testRecreateFsIDsRespMsg()
{
   FsckDirEntryList dentries;
   DatabaseTk::createDummyFsckDirEntries(4, &dentries);

   RecreateFsIDsRespMsg msg(&dentries);
   RecreateFsIDsRespMsg clone;

   CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
}

void TestMsgSerialization::testUpdateDirAttribsMsg()
{
   FsckDirInodeList dirs;
   DatabaseTk::createDummyFsckDirInodes(4, &dirs);

   UpdateDirAttribsMsg msg(&dirs);
   UpdateDirAttribsMsg clone;

   CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
}

void TestMsgSerialization::testUpdateDirAttribsRespMsg()
{
   FsckDirInodeList dirs;
   DatabaseTk::createDummyFsckDirInodes(4, &dirs);

   UpdateDirAttribsRespMsg msg(&dirs);
   UpdateDirAttribsRespMsg clone;

   CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
}

void TestMsgSerialization::testUpdateFileAttribsMsg()
{
   FsckFileInodeList files;
   DatabaseTk::createDummyFsckFileInodes(4, &files);

   UpdateFileAttribsMsg msg(&files);
   UpdateFileAttribsMsg clone;

   CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
}

void TestMsgSerialization::testUpdateFileAttribsRespMsg()
{
   FsckFileInodeList files;
   DatabaseTk::createDummyFsckFileInodes(4, &files);

   UpdateFileAttribsRespMsg msg(&files);
   UpdateFileAttribsRespMsg clone;

   CPPUNIT_ASSERT(testMsgSerialization(msg, clone) );
}
