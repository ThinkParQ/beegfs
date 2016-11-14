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
   CPPUNIT_TEST_SUITE(TestMsgSerialization);

   CPPUNIT_TEST(testAdjustChunkPermissionsMsgSerialization);
   CPPUNIT_TEST(testAdjustChunkPermissionsRespMsgSerialization);
   CPPUNIT_TEST(testCreateDefDirInodesMsg);
   CPPUNIT_TEST(testCreateDefDirInodesRespMsg);
   CPPUNIT_TEST(testCreateEmptyContDirsMsg);
   CPPUNIT_TEST(testCreateEmptyContDirsRespMsg);
   CPPUNIT_TEST(testDeleteChunksMsg);
   CPPUNIT_TEST(testDeleteChunksRespMsg);
   CPPUNIT_TEST(testDeleteDirEntriesMsg);
   CPPUNIT_TEST(testDeleteDirEntriesRespMsg);
   CPPUNIT_TEST(testFetchFsckChunkListMsgSerialization);
   CPPUNIT_TEST(testFetchFsckChunkListRespMsgSerialization);
   CPPUNIT_TEST(testFixInodeOwnersInDentryMsg);
   CPPUNIT_TEST(testFixInodeOwnersInDentryRespMsg);
   CPPUNIT_TEST(testFixInodeOwnersMsg);
   CPPUNIT_TEST(testFixInodeOwnersRespMsg);
   CPPUNIT_TEST(testFsckSetEventLoggingMsg);
   CPPUNIT_TEST(testFsckSetEventLoggingRespMsg);
   CPPUNIT_TEST(testLinkToLostAndFoundMsg);
   CPPUNIT_TEST(testLinkToLostAndFoundRespMsg);
   CPPUNIT_TEST(testMoveChunkFileMsg);
   CPPUNIT_TEST(testMoveChunkFileRespMsg);
   CPPUNIT_TEST(testRecreateDentriesMsg);
   CPPUNIT_TEST(testRecreateDentriesRespMsg);
   CPPUNIT_TEST(testRecreateFsIDsMsg);
   CPPUNIT_TEST(testRecreateFsIDsRespMsg);
   CPPUNIT_TEST(testRemoveInodesMsg);
   CPPUNIT_TEST(testRemoveInodesRespMsg);
   CPPUNIT_TEST(testRetrieveDirEntriesMsgSerialization);
   CPPUNIT_TEST(testRetrieveDirEntriesRespMsgSerialization);
   CPPUNIT_TEST(testRetrieveFsIDsMsgSerialization);
   CPPUNIT_TEST(testRetrieveFsIDsRespMsgSerialization);
   CPPUNIT_TEST(testRetrieveInodesMsgSerialization);
   CPPUNIT_TEST(testRetrieveInodesRespMsgSerialization);
   CPPUNIT_TEST(testUpdateDirAttribsMsg);
   CPPUNIT_TEST(testUpdateDirAttribsRespMsg);
   CPPUNIT_TEST(testUpdateFileAttribsMsg);
   CPPUNIT_TEST(testUpdateFileAttribsRespMsg);

   CPPUNIT_TEST_SUITE_END();

   public:
      TestMsgSerialization();

      void testAdjustChunkPermissionsMsgSerialization();
      void testAdjustChunkPermissionsRespMsgSerialization();
      void testCreateDefDirInodesMsg();
      void testCreateDefDirInodesRespMsg();
      void testCreateEmptyContDirsMsg();
      void testCreateEmptyContDirsRespMsg();
      void testDeleteChunksMsg();
      void testDeleteChunksRespMsg();
      void testDeleteDirEntriesMsg();
      void testDeleteDirEntriesRespMsg();
      void testFetchFsckChunkListMsgSerialization();
      void testFetchFsckChunkListRespMsgSerialization();
      void testFixInodeOwnersInDentryMsg();
      void testFixInodeOwnersInDentryRespMsg();
      void testFixInodeOwnersMsg();
      void testFixInodeOwnersRespMsg();
      void testFsckSetEventLoggingMsg();
      void testFsckSetEventLoggingRespMsg();
      void testLinkToLostAndFoundMsg();
      void testLinkToLostAndFoundRespMsg();
      void testMoveChunkFileMsg();
      void testMoveChunkFileRespMsg();
      void testRecreateDentriesMsg();
      void testRecreateDentriesRespMsg();
      void testRecreateFsIDsMsg();
      void testRecreateFsIDsRespMsg();
      void testRemoveInodesMsg();
      void testRemoveInodesRespMsg();
      void testRetrieveDirEntriesMsgSerialization();
      void testRetrieveDirEntriesRespMsgSerialization();
      void testRetrieveFsIDsMsgSerialization();
      void testRetrieveFsIDsRespMsgSerialization();
      void testRetrieveInodesMsgSerialization();
      void testRetrieveInodesRespMsgSerialization();
      void testUpdateDirAttribsMsg();
      void testUpdateDirAttribsRespMsg();
      void testUpdateFileAttribsMsg();
      void testUpdateFileAttribsRespMsg();

   private:
      LogContext log;
};

#endif /* TESTMSGSERIALIZATION_H_ */
