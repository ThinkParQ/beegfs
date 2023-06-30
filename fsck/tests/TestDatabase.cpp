#include "TestDatabase.h"

#include <common/net/message/nodes/HeartbeatRequestMsg.h>
#include <common/net/message/nodes/HeartbeatMsg.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/ListTk.h>
#include <database/FsckDBException.h>
#include <database/FsckDBTable.h>
#include <net/message/NetMessageFactory.h>
#include <program/Program.h>
#include <toolkit/DatabaseTk.h>
#include <toolkit/FsckTkEx.h>

// these tests currently require lots of friend access to fsck classes from common. put all the
// code into TestDatabase to allow these friend accesses.
#define DB_TEST(id) \
   TEST_F(TestDatabase, id) { TestDatabase::id(); } \
   void TestDatabase::id()

TestDatabase::TestDatabase()
{
   databasePath = "./fsckdb.test.dir";
}

TestDatabase::~TestDatabase()
{
}

void TestDatabase::SetUp()
{
   // create a new database
   Path dbPath(databasePath);
   bool createRes = StorageTk::createPathOnDisk(dbPath, false);
   if (!createRes)
      throw FsckDBException("Unable to create path to database");

   this->db.reset(new FsckDB(databasePath, 1024 * 1024, 1000, true) );
   this->db->clear();
}

void TestDatabase::TearDown()
{
   this->db.reset();
   StorageTk::removeDirRecursive(this->databasePath);
}

DB_TEST(testInsertAndReadSingleDentry)
{
   FsckDirEntryList dentryListIn(1, DatabaseTk::createDummyFsckDirEntry() );

   // test with small name, long name, and another long name at different offset in the name dump
   // file
   dentryListIn.front().setName("0");
   for(int i = 0; i < 3; i++)
   {
      this->db->getDentryTable()->clear();
      this->db->getDentryTable()->insert(dentryListIn);

      FsckDirEntryList dentryListOut = drain<FsckDirEntry>(this->db->getDentryTable()->get() );

      // names are not resolved by a simple load
      dentryListOut.front().setName(
         this->db->getDentryTable()->getNameOf(
            this->db->getDentryTable()->getAnyFor(
               db::EntryID::fromStr(dentryListIn.front().getID() ) ).second) );

      ASSERT_EQ(dentryListIn.front(), dentryListOut.front());

      dentryListIn.front().setName(std::string(255, "abc"[i]) );
   }
}

DB_TEST(testUpdateDentries)
{
   unsigned NUM_ROWS = 5;

   FsckDirEntryList dentryListIn;

   DatabaseTk::createDummyFsckDirEntries(NUM_ROWS, &dentryListIn);

   this->db->getDentryTable()->insert(dentryListIn);

   for (FsckDirEntryListIter iter = dentryListIn.begin(); iter != dentryListIn.end(); iter++)
      iter->setInodeOwnerNodeID(NumNodeID(iter->getInodeOwnerNodeID().val() + 1000) );

   this->db->getDentryTable()->updateFieldsExceptParent(dentryListIn);

   FsckDirEntryList dentryListOut = drain<FsckDirEntry>(this->db->getDentryTable()->get() );
   // names are not resolved by a simple load
   for(FsckDirEntryListIter it = dentryListOut.begin(); it != dentryListOut.end(); ++it)
      it->setName(
         this->db->getDentryTable()->getNameOf(
            this->db->getDentryTable()->getAnyFor(
               db::EntryID::fromStr(it->getID() ) ).second) );

   bool compareRes = ListTk::listsEqual(&dentryListIn, &dentryListOut);

   ASSERT_TRUE(compareRes) << "Comparision of dirEntries failed";
}

DB_TEST(testUpdateDirInodes)
{
   unsigned NUM_ROWS = 5;

   FsckDirInodeList inodeListIn;

   DatabaseTk::createDummyFsckDirInodes(NUM_ROWS, &inodeListIn);

   this->db->getDirInodesTable()->insert(inodeListIn);

   for (FsckDirInodeListIter iter = inodeListIn.begin(); iter != inodeListIn.end(); iter++)
      iter->setParentNodeID(NumNodeID(iter->getParentNodeID().val() + 1000) );

   this->db->getDirInodesTable()->update(inodeListIn);

   FsckDirInodeList inodeListOut = drain<FsckDirInode>(this->db->getDirInodesTable()->get() );

   bool compareRes = ListTk::listsEqual(&inodeListIn, &inodeListOut);
   ASSERT_TRUE(compareRes) << "Comparision of dir inodes failed";
}

DB_TEST(testUpdateFileInodes)
{
   unsigned NUM_ROWS = 15;

   FsckFileInodeList inodeListIn;

   DatabaseTk::createDummyFsckFileInodes(NUM_ROWS, &inodeListIn);

   this->db->getFileInodesTable()->insert(inodeListIn);

   for (FsckFileInodeListIter iter = inodeListIn.begin(); iter != inodeListIn.end(); iter++)
      iter->parentNodeID = NumNodeID(iter->getParentNodeID().val() + 1000);

   this->db->getFileInodesTable()->update(inodeListIn);

   std::list<db::FileInode> inodeListOut;
   std::list<db::StripeTargets> targetsListOut;

   drainToList(this->db->getFileInodesTable()->getInodes(), inodeListOut);
   drainToList(this->db->getFileInodesTable()->getTargets(), targetsListOut);

   while (!inodeListIn.empty() )
   {
      db::FileInode inode = inodeListOut.front();
      std::vector<uint16_t> targetsFound;

      ASSERT_EQ(inode.stripePatternSize, inode.id.sequence);
      int inlineTargets = std::min<int>(inode.id.sequence, inode.NTARGETS);

      targetsFound.resize(inode.stripePatternSize);
      std::copy(inode.targets, inode.targets + inlineTargets, targetsFound.begin() );

      inodeListIn.front().stripeTargets.clear();

      ASSERT_EQ(inodeListIn.front(), inode.toInodeWithoutStripes());

      while (!targetsListOut.empty() && targetsListOut.front().id == inode.id)
      {
         db::StripeTargets& st = targetsListOut.front();

         int outlineTargets = std::min<int>(
            inode.stripePatternSize - st.firstTargetIndex,
            st.NTARGETS);

         std::copy(
            st.targets, st.targets + outlineTargets,
            targetsFound.begin() + st.firstTargetIndex);

         targetsListOut.pop_front();
      }

      for (unsigned i = 0; i < targetsFound.size(); i++)
         ASSERT_EQ(targetsFound[i], inode.id.sequence + 1000 * (i + 1));

      inodeListIn.pop_front();
      inodeListOut.pop_front();
   }
}

DB_TEST(testUpdateChunks)
{
   unsigned NUM_ROWS = 5;

   FsckChunkList chunkListIn;

   DatabaseTk::createDummyFsckChunks(NUM_ROWS, &chunkListIn);

   this->db->getChunksTable()->insert(chunkListIn);

   for (FsckChunkListIter iter = chunkListIn.begin(); iter != chunkListIn.end(); iter++)
      iter->setUserID(iter->getUserID() + 1000);

   this->db->getChunksTable()->update(chunkListIn);

   FsckChunkList chunkListOut = drain<FsckChunk>(this->db->getChunksTable()->get() );

   bool compareRes = ListTk::listsEqual(&chunkListIn, &chunkListOut);
   ASSERT_TRUE(compareRes) << "Comparision of chunks failed";
}

DB_TEST(testInsertAndReadSingleFileInode)
{
   FsckFileInodeList inodeListIn(1, DatabaseTk::createDummyFsckFileInode() );

   this->db->getFileInodesTable()->insert(inodeListIn);

   std::list<db::FileInode> inodeListOut;
   std::list<db::StripeTargets> targetsListOut;

   drainToList(this->db->getFileInodesTable()->getInodes(), inodeListOut);
   drainToList(this->db->getFileInodesTable()->getTargets(), targetsListOut);

   ASSERT_EQ(inodeListOut.size(), 1u);
   ASSERT_EQ(targetsListOut.size(), 1u);

   db::FileInode inode = inodeListOut.front();

   ASSERT_EQ(inode.stripePatternSize, 7u);
   ASSERT_EQ(inode.targets[0], 7 + 1000);
   ASSERT_EQ(inode.targets[1], 7 + 2000);
   ASSERT_EQ(inode.targets[2], 7 + 3000);
   ASSERT_EQ(inode.targets[3], 7 + 4000);
   ASSERT_EQ(inode.targets[4], 7 + 5000);
   ASSERT_EQ(inode.targets[5], 7 + 6000);

   inodeListIn.front().stripeTargets.clear();

   ASSERT_EQ(inodeListIn.front(), inode.toInodeWithoutStripes());

   ASSERT_EQ(targetsListOut.front().firstTargetIndex, 6);
   ASSERT_EQ(targetsListOut.front().id, inode.id);
   ASSERT_EQ(targetsListOut.front().targets[0], 7 + 7000);
}

DB_TEST(testInsertAndReadSingleDirInode)
{
   FsckDirInodeList dirInodeListIn(1, DatabaseTk::createDummyFsckDirInode() );

   this->db->getDirInodesTable()->insert(dirInodeListIn);

   FsckDirInodeList dirInodeListOut = drain<FsckDirInode>(this->db->getDirInodesTable()->get() );

   FsckDirInode inode = dirInodeListOut.front();

   bool compareRes = ListTk::listsEqual(&dirInodeListIn, &dirInodeListOut);
   ASSERT_TRUE(compareRes) << "Comparision of dir inodes failed";
}

DB_TEST(testInsertAndReadDirInodes)
{
   uint NUM_ROWS = 5;

   FsckDirInodeList dirInodeListIn;

   DatabaseTk::createDummyFsckDirInodes(NUM_ROWS, &dirInodeListIn);

   this->db->getDirInodesTable()->insert(dirInodeListIn);

   FsckDirInodeList dirInodeListOut = drain<FsckDirInode>(this->db->getDirInodesTable()->get() );

   bool compareRes = ListTk::listsEqual(&dirInodeListIn, &dirInodeListOut);
   ASSERT_TRUE(compareRes) << "Comparision of dir inodes failed";
}

DB_TEST(testInsertAndReadSingleChunk)
{
   FsckChunkList chunkListIn(1, DatabaseTk::createDummyFsckChunk() );

   this->db->getChunksTable()->insert(chunkListIn);

   FsckChunkList chunkListOut = drain<FsckChunk>(this->db->getChunksTable()->get() );

   ASSERT_EQ(chunkListIn.size(), chunkListOut.size());

   bool compareRes = ListTk::listsEqual(&chunkListIn, &chunkListOut);
   ASSERT_TRUE(compareRes) << "Comparision of chunks failed";
}

DB_TEST(testInsertAndReadChunks)
{
   uint NUM_ROWS = 5;

   FsckChunkList chunkListIn;

   DatabaseTk::createDummyFsckChunks(NUM_ROWS, &chunkListIn);

   this->db->getChunksTable()->insert(chunkListIn);

   FsckChunkList chunkListOut = drain<FsckChunk>(this->db->getChunksTable()->get() );

   bool compareRes = ListTk::listsEqual(&chunkListIn, &chunkListOut);
   ASSERT_TRUE(compareRes) << "Comparision of chunks failed";
}

DB_TEST(testInsertAndReadSingleContDir)
{
   FsckContDirList contDirListIn(1, DatabaseTk::createDummyFsckContDir() );

   this->db->getContDirsTable()->insert(contDirListIn);

   FsckContDirList contDirListOut = drain<FsckContDir>(this->db->getContDirsTable()->get() );

   bool compareRes = ListTk::listsEqual(&contDirListIn, &contDirListOut);
   ASSERT_TRUE(compareRes) << "Comparision of cont dirs failed";
}

DB_TEST(testInsertAndReadContDirs)
{
   unsigned NUM_ROWS = 5;

   FsckContDirList contDirListIn;

   DatabaseTk::createDummyFsckContDirs(NUM_ROWS, &contDirListIn);

   this->db->getContDirsTable()->insert(contDirListIn);

   FsckContDirList contDirListOut = drain<FsckContDir>(this->db->getContDirsTable()->get() );

   bool compareRes = ListTk::listsEqual(&contDirListIn, &contDirListOut);
   ASSERT_TRUE(compareRes) << "Comparision of cont dirs failed";
}

DB_TEST(testInsertAndReadSingleFsID)
{
   FsckFsIDList listIn(1, DatabaseTk::createDummyFsckFsID() );

   this->db->getFsIDsTable()->insert(listIn);

   FsckFsIDList listOut = drain<FsckFsID>(this->db->getFsIDsTable()->get() );

   bool compareRes = ListTk::listsEqual(&listIn, &listOut);
   ASSERT_TRUE(compareRes) << "Comparision of fs ids failed";
}

DB_TEST(testInsertAndReadFsIDs)
{
   unsigned NUM_ROWS = 5;

   FsckFsIDList listIn;

   DatabaseTk::createDummyFsckFsIDs(NUM_ROWS, &listIn);

   this->db->getFsIDsTable()->insert(listIn);

   FsckFsIDList listOut = drain<FsckFsID>(this->db->getFsIDsTable()->get() );

   bool compareRes = ListTk::listsEqual(&listIn, &listOut);
   ASSERT_TRUE(compareRes) << "Comparision of fs ids failed";
}

DB_TEST(testFindDuplicateInodeIDs)
{
   FsckDirInodeList dirs;
   FsckDirInodeList disposal;
   FsckFileInodeList files;

   DatabaseTk::createDummyFsckDirInodes(4, &dirs);
   DatabaseTk::createDummyFsckFileInodes(1, &files);
   files.back().saveNodeID = NumNodeID(2);

   DatabaseTk::createDummyFsckDirInodes(2, 1, &dirs);
   dirs.back().saveNodeID = NumNodeID(2);

   DatabaseTk::createDummyFsckFileInodes(4, 4, &files);
   DatabaseTk::createDummyFsckFileInodes(6, 1, &files);
   files.back().saveNodeID = NumNodeID(3);

   // create a duplicate inlined inode entry. we see those when hardlinks are used
   DatabaseTk::createDummyFsckFileInodes(42, 1, &files);
   files.push_back(files.back() );

   disposal.push_back(
      FsckDirInode("disposal", "", NumNodeID(1), NumNodeID(1), 0, 0, UInt16Vector(),
         FsckStripePatternType_RAID0, NumNodeID(1), false, true, false));
   disposal.push_back(
      FsckDirInode("disposal", "", NumNodeID(2), NumNodeID(2), 0, 0, UInt16Vector(),
         FsckStripePatternType_RAID0, NumNodeID(2), false, true, false));

   this->db->getDirInodesTable()->insert(dirs);
   this->db->getDirInodesTable()->insert(disposal);
   this->db->getFileInodesTable()->insert(files);

   Cursor<checks::DuplicatedInode> c = this->db->findDuplicateInodeIDs();

   ASSERT_TRUE(c.step());
   {
      std::set<FsckDuplicateInodeInfo> nodes;
      nodes.insert(FsckDuplicateInodeInfo(std::string("0-1-1"), std::string("0-2-1"), 3000, false, false, DirEntryType_DIRECTORY));
      nodes.insert(FsckDuplicateInodeInfo(std::string("0-1-1"), std::string("0-2-1"), 2, true, false, DirEntryType_REGULARFILE));

      ASSERT_EQ(c.get()->first, db::EntryID(0, 1, 1));
      ASSERT_EQ(c.get()->second.size(), 2u);
      ASSERT_EQ(c.get()->second, nodes);
   }

   ASSERT_TRUE(c.step());
   {
      std::set<FsckDuplicateInodeInfo> nodes;
      nodes.insert(FsckDuplicateInodeInfo(std::string("2-1-1"), std::string("2-2-1"), 3002, false, false, DirEntryType_DIRECTORY));
      nodes.insert(FsckDuplicateInodeInfo(std::string("2-1-1"), std::string("2-2-1"), 2, false, false, DirEntryType_DIRECTORY));

      ASSERT_EQ(c.get()->first, db::EntryID(2, 1, 1));
      ASSERT_EQ(c.get()->second.size(), 2u);
      ASSERT_EQ(c.get()->second, nodes);
   }

   ASSERT_TRUE(c.step());
   {
      std::set<FsckDuplicateInodeInfo> nodes;
      nodes.insert(FsckDuplicateInodeInfo(std::string("6-1-1"), std::string("6-2-1"), 2006, true, false, DirEntryType_REGULARFILE));
      nodes.insert(FsckDuplicateInodeInfo(std::string("6-1-1"), std::string("6-2-1"), 3, true, false, DirEntryType_REGULARFILE));

      ASSERT_EQ(c.get()->first, db::EntryID(6, 1, 1));
      ASSERT_EQ(c.get()->second.size(), 2u);
      ASSERT_EQ(c.get()->second, nodes);
   }

   ASSERT_FALSE(c.step());
}

DB_TEST(testFindDuplicateChunks)
{
   FsckChunkList chunks;

   DatabaseTk::createDummyFsckChunks(0, 1, 1, &chunks);
   DatabaseTk::createDummyFsckChunks(0, 1, 1, &chunks);
   chunks.back().buddyGroupID = 1;

   DatabaseTk::createDummyFsckChunks(10, 1, 1, &chunks);
   DatabaseTk::createDummyFsckChunks(10, 1, 1, &chunks);

   this->db->getChunksTable()->insert(chunks);

   Cursor<std::list<FsckChunk> > c = this->db->findDuplicateChunks();

   ASSERT_TRUE(c.step());
   {
      std::list<FsckChunk> dups = *c.get();

      ASSERT_EQ(dups.size(), 2u);
      ASSERT_EQ(dups.front().getID(), "0-1-1");
      ASSERT_TRUE(
         (dups.front().getBuddyGroupID() == 0) ^ (dups.back().getBuddyGroupID() == 0) );
   }

   ASSERT_TRUE(c.step());
   {
      std::list<FsckChunk> dups = *c.get();

      ASSERT_EQ(dups.size(), 2u);
      ASSERT_EQ(dups.front().getID(), "A-1-1");
   }

   ASSERT_FALSE(c.step());
}

DB_TEST(testFindDuplicateContDirs)
{
   std::list<FsckContDir> contDirs = {
      {"1-0-1", {}, false},

      {"2-0-1", NumNodeID(0), false},
      {"2-0-1", NumNodeID(1), false},

      {"3-0-1", {}, false},
      {"3-0-1", {}, true},
   };

   db->getContDirsTable()->insert(contDirs);

   auto duplicate = db->findDuplicateContDirs();

   ASSERT_TRUE(duplicate.step());
   ASSERT_EQ(duplicate.get()->front().id.str(), "2-0-1");
   ASSERT_EQ(duplicate.get()->size(), 2u);

   ASSERT_TRUE(duplicate.step());
   ASSERT_EQ(duplicate.get()->front().id.str(), "3-0-1");
   ASSERT_EQ(duplicate.get()->size(), 2u);

   ASSERT_FALSE(duplicate.step());
}

DB_TEST(testFindMismirroredDentries)
{
   std::list<FsckDirEntry> dentries = {
      {"1-0-1", {}, {}, {}, {}, FsckDirEntryType_REGULARFILE, false, {}, 0, 0, false},
      {"2-0-1", {}, {}, {}, {}, FsckDirEntryType_REGULARFILE, false, {}, 0, 0, false},
      {"3-0-1", {}, {}, {}, {}, FsckDirEntryType_REGULARFILE, false, {}, 0, 0, false},
      {"4-0-1", {}, {}, {}, {}, FsckDirEntryType_REGULARFILE, false, {}, 0, 0, true},
      {"5-0-1", {}, {}, {}, {}, FsckDirEntryType_REGULARFILE, false, {}, 0, 0, true},
      {"6-0-1", {}, {}, {}, {}, FsckDirEntryType_REGULARFILE, false, {}, 0, 0, true},

      {"7-0-1", {}, {}, {}, {}, FsckDirEntryType_REGULARFILE, false, {}, 0, 0, false},
      {"8-0-1", {}, {}, {}, {}, FsckDirEntryType_REGULARFILE, false, {}, 0, 0, true},

      {"11-0-1", {}, {}, {}, {}, FsckDirEntryType_DIRECTORY, false, {}, 0, 0, false},
      {"12-0-1", {}, {}, {}, {}, FsckDirEntryType_DIRECTORY, false, {}, 0, 0, false},
      {"13-0-1", {}, {}, {}, {}, FsckDirEntryType_DIRECTORY, false, {}, 0, 0, false},
      {"14-0-1", {}, {}, {}, {}, FsckDirEntryType_DIRECTORY, false, {}, 0, 0, true},
      {"15-0-1", {}, {}, {}, {}, FsckDirEntryType_DIRECTORY, false, {}, 0, 0, true},
      {"16-0-1", {}, {}, {}, {}, FsckDirEntryType_DIRECTORY, false, {}, 0, 0, true},

      {"17-0-1", {}, {}, {}, {}, FsckDirEntryType_DIRECTORY, false, {}, 0, 0, false},
      {"18-0-1", {}, {}, {}, {}, FsckDirEntryType_DIRECTORY, false, {}, 0, 0, true},
   };

   std::list<FsckFileInode> files = {
      {"1-0-1", {}, {}, {}, 0, 0, 0, 0, 0, {}, FsckStripePatternType_RAID0, 0, {}, 0, 0, false,
         false, true, false},
      {"2-0-1", {}, {}, {}, 0, 0, 0, 0, 0, {}, FsckStripePatternType_RAID0, 0, {}, 0, 0, false,
         true, true, false},
      {"4-0-1", {}, {}, {}, 0, 0, 0, 0, 0, {}, FsckStripePatternType_RAID0, 0, {}, 0, 0, false,
         true, true, false},
      {"5-0-1", {}, {}, {}, 0, 0, 0, 0, 0, {}, FsckStripePatternType_RAID0, 0, {}, 0, 0, false,
         false, true, false},

      {"7-0-1", {}, {}, {}, 0, 0, 0, 0, 0, {}, FsckStripePatternType_RAID0, 0, {}, 0, 0, false,
         true, true, false},
      {"8-0-1", {}, {}, {}, 0, 0, 0, 0, 0, {}, FsckStripePatternType_RAID0, 0, {}, 0, 0, false,
         false, true, false},
   };

   std::list<FsckDirInode> dirs = {
      {"11-0-1", {}, {}, {}, 0, 0, {}, FsckStripePatternType_RAID0, {}, false, true, false},
      {"12-0-1", {}, {}, {}, 0, 0, {}, FsckStripePatternType_RAID0, {}, true, true, false},
      {"14-0-1", {}, {}, {}, 0, 0, {}, FsckStripePatternType_RAID0, {}, true, true, false},
      {"15-0-1", {}, {}, {}, 0, 0, {}, FsckStripePatternType_RAID0, {}, false, true, false},

      {"17-0-1", {}, {}, {}, 0, 0, {}, FsckStripePatternType_RAID0, {}, true, true, false},
      {"18-0-1", {}, {}, {}, 0, 0, {}, FsckStripePatternType_RAID0, {}, false, true, false},
   };

   std::list<FsckModificationEvent> events = {
      {ModificationEvent_FILEMOVED, "7-0-1"},
      {ModificationEvent_FILEMOVED, "8-0-1"},

      {ModificationEvent_DIRMOVED, "17-0-1"},
      {ModificationEvent_DIRMOVED, "18-0-1"},
   };

   db->getDentryTable()->insert(dentries);
   db->getFileInodesTable()->insert(files);
   db->getDirInodesTable()->insert(dirs);
   db->getModificationEventsTable()->insert(events,
         db->getModificationEventsTable()->newBulkHandle());

   auto mismirrored = db->findMismirroredDentries();

   ASSERT_TRUE(mismirrored.step());
   ASSERT_EQ(mismirrored.get()->id.str(), "2-0-1");

   ASSERT_TRUE(mismirrored.step());
   ASSERT_EQ(mismirrored.get()->id.str(), "5-0-1");

   ASSERT_TRUE(mismirrored.step());
   ASSERT_EQ(mismirrored.get()->id.str(), "12-0-1");

   ASSERT_TRUE(mismirrored.step());
   ASSERT_EQ(mismirrored.get()->id.str(), "15-0-1");

   ASSERT_FALSE(mismirrored.step());
}

DB_TEST(testFindMismirroredDirectories)
{
   std::list<FsckDirInode> dirs = {
      {"1-0-1", {}, {}, {}, 0, 0, {}, FsckStripePatternType_RAID0, {}, false, true, false},
      {"2-0-1", {}, {}, {}, 0, 0, {}, FsckStripePatternType_RAID0, {}, true, true, false},
      {"3-0-1", {}, {}, {}, 0, 0, {}, FsckStripePatternType_RAID0, {}, false, true, false},

      {"4-0-1", {}, {}, {}, 0, 0, {}, FsckStripePatternType_RAID0, {}, false, true, false},
      {"5-0-1", {}, {}, {}, 0, 0, {}, FsckStripePatternType_RAID0, {}, true, true, false},
      {"6-0-1", {}, {}, {}, 0, 0, {}, FsckStripePatternType_RAID0, {}, false, true, false},

      {"7-0-1", {}, {}, {}, 0, 0, {}, FsckStripePatternType_RAID0, {}, false, true, false},
      {"8-0-1", {}, {}, {}, 0, 0, {}, FsckStripePatternType_RAID0, {}, true, true, false},
      {"9-0-1", {}, {}, {}, 0, 0, {}, FsckStripePatternType_RAID0, {}, false, true, false},

      {"10-0-1", {}, {}, {}, 0, 0, {}, FsckStripePatternType_RAID0, {}, false, true, true},
      {"11-0-1", {}, {}, {}, 0, 0, {}, FsckStripePatternType_RAID0, {}, true, true, true},
   };

   std::list<FsckContDir> contDirs = {
      {"1-0-1", {}, false},
      {"2-0-1", {}, true},

      {"4-0-1", {}, true},
      {"5-0-1", {}, false},

      {"7-0-1", {}, true},
      {"8-0-1", {}, false},

      {"10-0-1", {}, false},
      {"11-0-1", {}, true},
   };

   std::list<FsckModificationEvent> events = {
      {ModificationEvent_DIRMOVED, "7-0-1"},
      {ModificationEvent_DIRMOVED, "8-0-1"},
      {ModificationEvent_DIRMOVED, "9-0-1"},
   };

   db->getDirInodesTable()->insert(dirs);
   db->getContDirsTable()->insert(contDirs);
   db->getModificationEventsTable()->insert(events,
         db->getModificationEventsTable()->newBulkHandle());

   auto mismirrored = db->findMismirroredDirectories();

   ASSERT_TRUE(mismirrored.step());
   ASSERT_EQ(mismirrored.get()->id.str(), "4-0-1");

   ASSERT_TRUE(mismirrored.step());
   ASSERT_EQ(mismirrored.get()->id.str(), "5-0-1");

   ASSERT_TRUE(mismirrored.step());
   ASSERT_EQ(mismirrored.get()->id.str(), "10-0-1");

   ASSERT_TRUE(mismirrored.step());
   ASSERT_EQ(mismirrored.get()->id.str(), "11-0-1");

   ASSERT_FALSE(mismirrored.step());
}

DB_TEST(testCheckForAndInsertDanglingDirEntries)
{
   unsigned NUM_DENTRIES_DIRS = 5;
   unsigned NUM_DENTRIES_FILES = 5;

   unsigned MISSING_EACH = 2;

   // create dentries for dirs
   FsckDirEntryList dentriesDirs;
   DatabaseTk::createDummyFsckDirEntries(NUM_DENTRIES_DIRS, &dentriesDirs,
      FsckDirEntryType_DIRECTORY);

   // create dentries for files (start with ID after the NUM_DENTRIES_DIRS)
   FsckDirEntryList dentriesFiles;
   DatabaseTk::createDummyFsckDirEntries(NUM_DENTRIES_DIRS, NUM_DENTRIES_FILES, &dentriesFiles);

   // create dir inodes
   FsckDirInodeList dirInodes;
   DatabaseTk::createDummyFsckDirInodes(NUM_DENTRIES_DIRS-MISSING_EACH - 1, &dirInodes);

   // create file inodes
   FsckFileInodeList fileInodes;
   DatabaseTk::createDummyFsckFileInodes(NUM_DENTRIES_DIRS, NUM_DENTRIES_FILES-MISSING_EACH - 1,
      &fileInodes);

   this->db->getModificationEventsTable()->insert(
      FsckModificationEventList(1,
         FsckModificationEvent(ModificationEvent_FILEMOVED, dentriesFiles.rbegin()->getID() ) ),
      this->db->getModificationEventsTable()->newBulkHandle() );
   this->db->getModificationEventsTable()->insert(
      FsckModificationEventList(1,
         FsckModificationEvent(ModificationEvent_DIRMOVED, dentriesDirs.rbegin()->getID() ) ),
      this->db->getModificationEventsTable()->newBulkHandle() );

   this->db->getDentryTable()->insert(dentriesDirs);
   this->db->getDentryTable()->insert(dentriesFiles);

   // insert the inodes
   this->db->getFileInodesTable()->insert(fileInodes);
   this->db->getDirInodesTable()->insert(dirInodes);

   // now find all dentries without an inode into the errors table
   FsckDirEntryList entries;
   drainToList(this->db->findDanglingDirEntries(), entries);

   ASSERT_EQ(entries.size(), MISSING_EACH*2);
}

DB_TEST(testCheckForAndInsertInodesWithWrongOwner)
{
   unsigned NUM_INODES = 10;
   unsigned NUM_WRONG = 4;

   // create the inodes
   FsckDirInodeList dirInodes;
   DatabaseTk::createDummyFsckDirInodes(NUM_INODES, &dirInodes);

   // for all of the inodes set the same ownerNodeID and saveNodeID
   for ( FsckDirInodeListIter dirInodeIter = dirInodes.begin(); dirInodeIter != dirInodes.end();
      dirInodeIter++ )
   {
      dirInodeIter->ownerNodeID = NumNodeID(1000);
      dirInodeIter->saveNodeID = NumNodeID(1000);
   }

   // now set wrong ownerInfo for NUM_WRONG inodes
   unsigned i = 0;
   FsckDirInodeListIter iter = dirInodes.begin();
   while ( (iter != dirInodes.end()) && (i < NUM_WRONG) )
   {
      iter->ownerNodeID = NumNodeID(iter->ownerNodeID.val() + 1000);
      iter++;
      i++;
   }

   iter->ownerNodeID = NumNodeID(iter->ownerNodeID.val() + 1000);
   this->db->getModificationEventsTable()->insert(
      FsckModificationEventList(1,
         FsckModificationEvent(ModificationEvent_FILEMOVED, iter->getID() ) ),
      this->db->getModificationEventsTable()->newBulkHandle() );

   // save them to DB
   this->db->getDirInodesTable()->insert(dirInodes);

   // get cursor to the entries
   FsckDirInodeList inodes;

   drainToList(this->db->findInodesWithWrongOwner(), inodes);

   ASSERT_EQ(inodes.size(), NUM_WRONG);
}

DB_TEST(testCheckForAndInsertDirEntriesWithWrongOwner)
{
   unsigned NUM_DENTRIES_DIRS = 5;
   unsigned NUM_DENTRIES_FILES = 5;

   unsigned WRONG_EACH = 2;

   // create dentries for dirs
   FsckDirEntryList dentriesDirs;
   DatabaseTk::createDummyFsckDirEntries(NUM_DENTRIES_DIRS, &dentriesDirs,
      FsckDirEntryType_DIRECTORY);

   // create dentries for files (start with ID after the NUM_DENTRIES_DIRS)
   FsckDirEntryList dentriesFiles;
   DatabaseTk::createDummyFsckDirEntries(NUM_DENTRIES_DIRS, NUM_DENTRIES_FILES, &dentriesFiles);

   // create dir inodes
   FsckDirInodeList dirInodes;
   DatabaseTk::createDummyFsckDirInodes(NUM_DENTRIES_DIRS, &dirInodes);

   // for all of the inodes set the same saveNodeID
   for (FsckDirInodeListIter dirInodeIter = dirInodes.begin(); dirInodeIter != dirInodes.end();
      dirInodeIter++)
   {
      dirInodeIter->saveNodeID = NumNodeID(1000);
   }

   // create file inodes
   FsckFileInodeList fileInodes;
   DatabaseTk::createDummyFsckFileInodes(NUM_DENTRIES_DIRS, NUM_DENTRIES_FILES,
      &fileInodes);

   // for all of the inodes set the same saveNodeID
   for (FsckFileInodeListIter fileInodeIter = fileInodes.begin(); fileInodeIter != fileInodes.end();
      fileInodeIter++)
   {
      fileInodeIter->saveNodeID = NumNodeID(1000);
   }

   // now manipulate the first WRONG_EACH dir entries and file entries to have wrong owner
   // information and the rest to be correct
   unsigned i = 0;

   FsckDirEntryListIter iter = dentriesDirs.begin();
   while ( ( iter != dentriesDirs.end() ) && (i < WRONG_EACH) )
   {
      iter->inodeOwnerNodeID = NumNodeID(iter->inodeOwnerNodeID.val() + 1000);
      iter++;
      i++;
   }

   iter->inodeOwnerNodeID = NumNodeID(iter->inodeOwnerNodeID.val() + 1000);
   this->db->getModificationEventsTable()->insert(
      FsckModificationEventList(1,
         FsckModificationEvent(ModificationEvent_FILEMOVED, iter->getID() ) ),
      this->db->getModificationEventsTable()->newBulkHandle() );
   ++iter;

   while ( iter != dentriesDirs.end() )
   {
      iter->inodeOwnerNodeID = NumNodeID(1000);
      iter++;
      i++;
   }

   i = 0;
   iter = dentriesFiles.begin();
   while ( ( iter != dentriesFiles.end() ) && (i < WRONG_EACH) )
   {
      iter->inodeOwnerNodeID = NumNodeID(iter->inodeOwnerNodeID.val() + 1000);
      iter++;
      i++;
   }

   iter->inodeOwnerNodeID = NumNodeID(iter->inodeOwnerNodeID.val() + 1000);
   this->db->getModificationEventsTable()->insert(
      FsckModificationEventList(1,
         FsckModificationEvent(ModificationEvent_FILEMOVED, iter->getID() ) ),
      this->db->getModificationEventsTable()->newBulkHandle() );
   ++iter;

   while ( iter != dentriesFiles.end() )
   {
       iter->inodeOwnerNodeID = NumNodeID(1000);
       iter++;
       i++;
    }

   // insert the dentries
   this->db->getDentryTable()->insert(dentriesDirs);
   this->db->getDentryTable()->insert(dentriesFiles);

   // insert the inodes
   this->db->getFileInodesTable()->insert(fileInodes);
   this->db->getDirInodesTable()->insert(dirInodes);

   ASSERT_EQ(countCursor(this->db->findDirEntriesWithWrongOwner()), WRONG_EACH*2);
}

DB_TEST(testCheckForAndInsertMissingDentryByIDFile)
{
   unsigned NUM_DENTRIES = 50;
   unsigned NUM_DELETED_IDS = 5;
   unsigned NUM_WRONG_NODE_IDS = 5;
   unsigned NUM_WRONG_DEVICE_IDS = 5;
   unsigned NUM_WRONG_INODE_IDS = 5;

   unsigned ERRORS_TOTAL = NUM_DELETED_IDS + NUM_WRONG_NODE_IDS + NUM_WRONG_DEVICE_IDS
      + NUM_WRONG_INODE_IDS;

   // create dentries
   FsckDirEntryList dentries;
   DatabaseTk::createDummyFsckDirEntries(NUM_DENTRIES, &dentries);

   // create IDs
   FsckFsIDList fsIDs;
   DatabaseTk::createDummyFsckFsIDs(NUM_DENTRIES, &fsIDs);

   // make sure the data is correct
   FsckDirEntryListIter dentryIter = dentries.begin();
   FsckFsIDListIter fsIDIter = fsIDs.begin();
   for (uint i = 0; i< NUM_DENTRIES; i++)
   {
      dentryIter->saveNodeID = NumNodeID(i);
      dentryIter->parentDirID = "ff-ff-ff";
      dentryIter->saveDevice = i+1000;
      dentryIter->saveInode = i+2000;

      fsIDIter->saveNodeID = NumNodeID(i);
      fsIDIter->parentDirID = "ff-ff-ff";
      fsIDIter->saveDevice = i+1000;
      fsIDIter->saveInode = i+2000;

      dentryIter++;
      fsIDIter++;
   }

   // now manipulate the first NUM_WRONG_NODE_IDS fsids

   fsIDIter = fsIDs.begin();
   for (unsigned i = 0 ; i < NUM_WRONG_NODE_IDS; i++)
   {
      if (fsIDIter == fsIDs.end())
         FAIL() << "End of list reached! Most likely a programmer's error!";

      fsIDIter->saveNodeID = NumNodeID(fsIDIter->getSaveNodeID().val() + 1000);
      fsIDIter++;
   }

   // now manipulate NUM_WRONG_DEVICE_IDS fsids
   for (unsigned i = 0 ; i < NUM_WRONG_DEVICE_IDS; i++)
   {
      if (fsIDIter == fsIDs.end())
         FAIL() << "End of list reached! Most likely a programmer's error!";

      fsIDIter->saveDevice = fsIDIter->getSaveDevice() + 1000;
      fsIDIter++;
   }

   // now manipulate NUM_WRONG_INODE_IDS fsids
   for (unsigned i = 0 ; i < NUM_WRONG_INODE_IDS; i++)
   {
      if (fsIDIter == fsIDs.end())
         FAIL() << "End of list reached! Most likely a programmer's error!";

      fsIDIter->saveInode = fsIDIter->getSaveInode() + 1000;
      fsIDIter++;
   }

   // now delete NUM_DELETE_IDS fsids
   for (unsigned i = 0 ; i < NUM_DELETED_IDS; i++)
   {
      if (fsIDIter == fsIDs.end())
         FAIL() << "End of list reached! Most likely a programmer's error!";

      fsIDIter = fsIDs.erase(fsIDIter);
   }

   this->db->getModificationEventsTable()->insert(
      FsckModificationEventList(1,
         FsckModificationEvent(ModificationEvent_FILEMOVED, fsIDIter->getID() ) ),
      this->db->getModificationEventsTable()->newBulkHandle() );
   fsIDs.erase(fsIDIter);

   this->db->getDentryTable()->insert(dentries);
   this->db->getFsIDsTable()->insert(fsIDs);

   FsckDirEntryList entries;
   drainToList(this->db->findDirEntriesWithBrokenByIDFile(), entries);

   ASSERT_EQ(entries.size(), ERRORS_TOTAL);
}

DB_TEST(testCheckForAndInsertOrphanedDentryByIDFiles)
{
   unsigned NUM_DENTRIES = 50;
   unsigned NUM_DELETES = 5;

   // create dentries
   FsckDirEntryList dentries;
   DatabaseTk::createDummyFsckDirEntries(NUM_DENTRIES, &dentries);

   // create IDs
   FsckFsIDList fsIDs;
   DatabaseTk::createDummyFsckFsIDs(NUM_DENTRIES, &fsIDs);

   // make sure the data is correct
   FsckDirEntryListIter dentryIter = dentries.begin();
   FsckFsIDListIter fsIDIter = fsIDs.begin();
   for (uint i = 0; i< NUM_DENTRIES; i++)
   {
      dentryIter->saveNodeID = NumNodeID(i);
      dentryIter->parentDirID = "ff-ff-ff";
      dentryIter->saveDevice = i+1000;
      dentryIter->saveInode = i+2000;

      fsIDIter->saveNodeID = NumNodeID(i);
      fsIDIter->parentDirID = "ff-ff-ff";
      fsIDIter->saveDevice = i+1000;
      fsIDIter->saveInode = i+2000;

      dentryIter++;
      fsIDIter++;
   }

   // now delete NUM_DELETES dentry, plus one for online modification
   dentryIter = dentries.begin();
   for (unsigned i = 0 ; i < NUM_DELETES + 1; i++)
   {
      if (dentryIter == dentries.end())
         FAIL() << "End of list reached! Most likely a programmer's error!";

      if(i == NUM_DELETES)
         this->db->getModificationEventsTable()->insert(
            FsckModificationEventList(1,
               FsckModificationEvent(ModificationEvent_DIRMOVED, dentryIter->getID() ) ),
      this->db->getModificationEventsTable()->newBulkHandle() );

      dentryIter = dentries.erase(dentryIter);
   }

   this->db->getDentryTable()->insert(dentries);
   this->db->getFsIDsTable()->insert(fsIDs);

   fsIDs.clear();
   drainToList(this->db->findOrphanedFsIDFiles(), fsIDs);

   ASSERT_EQ(fsIDs.size(), NUM_DELETES);
}

DB_TEST(testCheckForAndInsertOrphanedDirInodes)
{
   unsigned NUM_INODES = 5;
   unsigned MISSING_DENTRIES = 2;

   // create dir inodes
   FsckDirInodeList dirInodes;
   DatabaseTk::createDummyFsckDirInodes(NUM_INODES, &dirInodes);

   // create dentries for dirs
   FsckDirEntryList dentriesDirs;
   DatabaseTk::createDummyFsckDirEntries(NUM_INODES-MISSING_DENTRIES - 1, &dentriesDirs,
      FsckDirEntryType_DIRECTORY);

   // create some dentries for files (just to see what happens if they are present)
   FsckDirEntryList dentriesFiles;
   DatabaseTk::createDummyFsckDirEntries(NUM_INODES, NUM_INODES+NUM_INODES, &dentriesFiles);

   this->db->getModificationEventsTable()->insert(
      FsckModificationEventList(1,
         FsckModificationEvent(ModificationEvent_DIRMOVED, dirInodes.rbegin()->getID() ) ),
      this->db->getModificationEventsTable()->newBulkHandle() );

   // insert the dentries
   this->db->getDentryTable()->insert(dentriesDirs);
   this->db->getDentryTable()->insert(dentriesFiles);
   this->db->getDirInodesTable()->insert(dirInodes);

   FsckDirInodeList inodes;
   drainToList(this->db->findOrphanedDirInodes(), inodes);

   ASSERT_EQ(inodes.size(), MISSING_DENTRIES);
}

DB_TEST(testCheckForAndInsertOrphanedFileInodes)
{
   unsigned NUM_INODES = 5;
   unsigned MISSING_DENTRIES = 2;

   // create file inodes
   FsckFileInodeList fileInodes;
   DatabaseTk::createDummyFsckFileInodes(NUM_INODES, &fileInodes);

   // create dentries for files
   FsckDirEntryList dentriesFiles;
   DatabaseTk::createDummyFsckDirEntries(NUM_INODES-MISSING_DENTRIES - 1, &dentriesFiles);

   // create some dentries for dirs (just to see what happens if they are present)
   FsckDirEntryList dentriesDirs;
   DatabaseTk::createDummyFsckDirEntries(NUM_INODES, NUM_INODES+NUM_INODES, &dentriesDirs,
      FsckDirEntryType_DIRECTORY);

   this->db->getModificationEventsTable()->insert(
      FsckModificationEventList(1,
         FsckModificationEvent(ModificationEvent_DIRMOVED, fileInodes.rbegin()->getID() ) ),
      this->db->getModificationEventsTable()->newBulkHandle() );

   // insert the dentries
   this->db->getDentryTable()->insert(dentriesDirs);
   this->db->getDentryTable()->insert(dentriesFiles);
   this->db->getFileInodesTable()->insert(fileInodes);

   FsckFileInodeList inodes;
   drainToList(this->db->findOrphanedFileInodes(), inodes);

   ASSERT_EQ(inodes.size(), MISSING_DENTRIES);
}

DB_TEST(testCheckForAndInsertOrphanedChunks)
{
   static_assert(db::FileInode::NTARGETS < 20, "enlarge 1-0-1 stripe pattern");

   std::list<FsckFileInode> inodes = {
      { "0-0-1", "root", NumNodeID(), PathInfo(), 0, 0, 0, 0, 0, {1, 2},
         FsckStripePatternType_RAID0, 0, NumNodeID(), 0, 0, false, false, false, false },
      { "1-0-1", "root", NumNodeID(), PathInfo(), 0, 0, 0, 0, 0, {11, 12, 13, 14, 15, 16, 17, 18,
               19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30}, FsckStripePatternType_BUDDYMIRROR,
            0, NumNodeID(), 0, 0, false, false, false, false },

      { "2-0-1", "root", NumNodeID(), PathInfo(), 0, 0, 0, 0, 0, {1, 2},
         FsckStripePatternType_RAID0, 0, NumNodeID(), 0, 0, false, false, false, false },
   };

   std::list<FsckChunk> goodChunks = {
      { "0-0-1", 1, Path(), 0, 0, 0, 0, 0, 0, 0, 0 },
      { "0-0-1", 2, Path(), 0, 0, 0, 0, 0, 0, 0, 0 },

      { "1-0-1", 0, Path(), 0, 0, 0, 0, 0, 0, 0, 11 },
      { "1-0-1", 0, Path(), 0, 0, 0, 0, 0, 0, 0, 12 },
   };

   std::list<FsckChunk> badChunks = {
      { "0-0-1", 3, Path(), 0, 0, 0, 0, 0, 0, 0, 0 },
      { "1-0-1", 0, Path(), 0, 0, 0, 0, 0, 0, 0, 10 },
      { "3-0-1", 1, Path(), 0, 0, 0, 0, 0, 0, 0, 0 },
      { "3-0-1", 2, Path(), 0, 0, 0, 0, 0, 0, 0, 0 },
   };

   db->getFileInodesTable()->insert(inodes);
   db->getChunksTable()->insert(goodChunks);
   db->getChunksTable()->insert(badChunks);

   db->getModificationEventsTable()->insert(
      FsckModificationEventList(1,
         {ModificationEvent_FILEMOVED, "2-0-1"}),
      db->getModificationEventsTable()->newBulkHandle());

   std::list<FsckChunk> result;
   drainToList(db->findOrphanedChunks(), result);

   ASSERT_EQ(result, badChunks);
}

DB_TEST(testCheckForAndInsertInodesWithoutContDir)
{
   unsigned NUM_INODES = 5;
   unsigned MISSING_CONTDIRS = 2;

   // create dir inodes
   FsckDirInodeList dirInodes;
   DatabaseTk::createDummyFsckDirInodes(NUM_INODES, &dirInodes);

   // create cont dirs
   FsckContDirList contDirs;
   DatabaseTk::createDummyFsckContDirs(NUM_INODES-MISSING_CONTDIRS - 1, &contDirs);

   this->db->getModificationEventsTable()->insert(
      FsckModificationEventList(1,
         FsckModificationEvent(ModificationEvent_DIRMOVED, dirInodes.rbegin()->getID() ) ),
      this->db->getModificationEventsTable()->newBulkHandle() );

   this->db->getDirInodesTable()->insert(dirInodes);
   this->db->getContDirsTable()->insert(contDirs);

   FsckDirInodeList inodes;
   drainToList(this->db->findInodesWithoutContDir(), inodes);

   ASSERT_EQ(inodes.size(), MISSING_CONTDIRS);
}

DB_TEST(testCheckForAndInsertOrphanedContDirs)
{
   unsigned NUM_CONTDIRS = 5;
   unsigned MISSING_INODES = 2;

   // create cont dirs
   FsckContDirList contDirs;
   DatabaseTk::createDummyFsckContDirs(NUM_CONTDIRS, &contDirs);

   // create dir inodes
   FsckDirInodeList dirInodes;
   DatabaseTk::createDummyFsckDirInodes(NUM_CONTDIRS-MISSING_INODES - 1, &dirInodes);

   this->db->getModificationEventsTable()->insert(
      FsckModificationEventList(1,
         FsckModificationEvent(ModificationEvent_DIRMOVED, contDirs.rbegin()->getID() ) ),
      this->db->getModificationEventsTable()->newBulkHandle() );

   this->db->getContDirsTable()->insert(contDirs);
   this->db->getDirInodesTable()->insert(dirInodes);

   contDirs.clear();
   drainToList(this->db->findOrphanedContDirs(), contDirs);

   ASSERT_EQ(contDirs.size(), MISSING_INODES);
}

DB_TEST(testCheckForAndInsertFileInodesWithWrongAttribs)
{
   unsigned NUM_INODES = 10;
   unsigned NUM_CHUNKS_PER_INODE = 2;

   unsigned WRONG_DATA_INODES_SIZE = 2;
   unsigned WRONG_DATA_INODES_HARDLINKS = 2;
   unsigned WRONG_DATA_NO_CHUNKS = 1;

   unsigned WRONG_DATA_INODES = WRONG_DATA_INODES_SIZE + WRONG_DATA_INODES_HARDLINKS
      + WRONG_DATA_NO_CHUNKS;

   int64_t EXPECTED_FILE_SIZE = 100000; // take a value that can be divided by NUM_CHUNKS_PER_INODE
   unsigned EXPECTED_HARDLINKS = 1;

   // create file inodes
   FsckFileInodeList inodesIn;
   DatabaseTk::createDummyFsckFileInodes(NUM_INODES, &inodesIn);

   // create chunks
   FsckChunkList chunks;
   DatabaseTk::createDummyFsckChunks(NUM_INODES, NUM_CHUNKS_PER_INODE, &chunks);

   // create dentries
   FsckDirEntryList dentries;
   DatabaseTk::createDummyFsckDirEntries(NUM_INODES, &dentries);

   // add another dentry with the ID of the last dentry to simulate a hardlink
   FsckDirEntry dentry = dentries.back();
   dentry.name = "hardlink";

   dentries.push_back(dentry);

   // make sure stat attrs do match
   for ( FsckFileInodeListIter iter = inodesIn.begin(); iter != inodesIn.end(); iter++ )
   {
      iter->fileSize = EXPECTED_FILE_SIZE;
      iter->numHardLinks = EXPECTED_HARDLINKS;

      if (iter->getID() == dentry.getID())
         iter->numHardLinks = iter->getNumHardLinks()+1;
   }

   for ( FsckChunkListIter iter = chunks.begin(); iter != chunks.end(); iter++ )
   {
      FsckChunk chunk = *iter;
      iter->fileSize = EXPECTED_FILE_SIZE/NUM_CHUNKS_PER_INODE;
   }

   // we hold a list of modifed IDs; each time we modify attributes for a chunk we add its ID
   // we do this to make sure, that if we modify x inodes, they are x different ones
   StringList modifiedIDs;

   // now manipulate data of WRONG_DATA_INODES_SIZE chunks
   FsckChunkListIter chunkIter = chunks.begin();
   // only manipulate chunks from one target
   uint16_t targetID = chunkIter->getTargetID();
   unsigned i = 0;
   while (i < WRONG_DATA_INODES_SIZE)
   {
      if (chunkIter == chunks.end())
         break;

      if (chunkIter->getTargetID() == targetID)
      {
         StringListIter tmpIter = std::find(modifiedIDs.begin(), modifiedIDs.end(),
            chunkIter->getID());

         if (tmpIter == modifiedIDs.end())
         {
            modifiedIDs.push_back(chunkIter->getID());
            chunkIter->fileSize = chunkIter->fileSize*2;
            i++;
         }
      }

      chunkIter++;
   }

   // now manipulate data for WRONG_DATA_INODES_HARDLINKS inodes
   FsckFileInodeListIter inodeIter = inodesIn.begin();

   i = 0;
   while ( i < WRONG_DATA_INODES_HARDLINKS )
   {
      if ( inodeIter == inodesIn.end() )
         break;

      StringListIter tmpIter = std::find(modifiedIDs.begin(), modifiedIDs.end(),
         inodeIter->getID());

      if ( tmpIter == modifiedIDs.end() )
      {
         modifiedIDs.push_back(inodeIter->getID());
         inodeIter->numHardLinks = i == 0 ? 0 : 100;
         i++;
      }

      inodeIter++;
   }

   i = 0;
   while(i < WRONG_DATA_NO_CHUNKS)
   {
      const std::string& id = inodeIter->getID();

      for(FsckChunkListIter it = chunks.begin(); it != chunks.end(); ++it)
      {
         if(it->getID() == id)
         {
            FsckChunkListIter cur = it;
            ++it;

            chunks.erase(cur);
            --it;
         }
      }

      modifiedIDs.push_back(id);
      inodeIter++;
      i++;
   }

   inodeIter->fileSize = inodeIter->getFileSize() + 1;
   this->db->getModificationEventsTable()->insert(
      FsckModificationEventList(1,
         FsckModificationEvent(ModificationEvent_FILEMOVED, inodeIter->getID() ) ),
      this->db->getModificationEventsTable()->newBulkHandle() );

   this->db->getFileInodesTable()->insert(inodesIn);
   this->db->getChunksTable()->insert(chunks);
   this->db->getDentryTable()->insert(dentries);

   ASSERT_EQ(countCursor(this->db->findWrongInodeFileAttribs()), WRONG_DATA_INODES);
}

DB_TEST(testCheckForAndInsertDirInodesWithWrongAttribs)
{
   unsigned NUM_INODES = 10;

   unsigned WRONG_DATA_SIZE = 2;
   unsigned WRONG_DATA_LINKS = 2;

   unsigned WRONG_DATA_INODES = WRONG_DATA_SIZE + WRONG_DATA_LINKS;

   int64_t EXPECTED_SIZE = 2;
   unsigned EXPECTED_LINKS = 2; // no subdirs present, so 2 links expected

   // create dir inodes
   FsckDirInodeList inodes;
   DatabaseTk::createDummyFsckDirInodes(NUM_INODES, &inodes);

   // create dentries
   FsckDirEntryList dentries;
   DatabaseTk::createDummyFsckDirEntries(NUM_INODES*EXPECTED_LINKS, &dentries);

   // set parent info, so that it matches
   FsckDirEntryListIter dentryIter = dentries.begin();
   FsckDirInodeListIter inodeIter = inodes.begin();

   while ( inodeIter != inodes.end() )
   {
      inodeIter->setSize(EXPECTED_SIZE);
      inodeIter->setNumHardLinks(EXPECTED_LINKS);

      for ( int i = 0; i < EXPECTED_SIZE; i++ )
      {
         dentryIter->setParentDirID(inodeIter->getID());
         dentryIter++;
      }
      inodeIter++;
   }

   // we hold a list of modifed IDs; each time we modify attributes we add the ID of the inode
   // we do this to make sure, that if we modify x inodes, they are x different ones
   StringList modifiedIDs;

   // now manipulate the size
   inodeIter = inodes.begin();

   unsigned i = 0;
   while ( i < WRONG_DATA_SIZE )
   {
      if ( inodeIter == inodes.end() )
         break;

      StringListIter tmpIter = std::find(modifiedIDs.begin(), modifiedIDs.end(),
         inodeIter->getID());

      if ( tmpIter == modifiedIDs.end() )
      {
         modifiedIDs.push_back(inodeIter->getID());
         // set new size
         inodeIter->setSize(inodeIter->getSize()*10);
         i++;
      }

      inodeIter++;
   }

   // now manipulate link count
   i = 0;
   while ( i < WRONG_DATA_LINKS )
   {
      if ( inodeIter == inodes.end() )
         break;

      StringListIter tmpIter = std::find(modifiedIDs.begin(), modifiedIDs.end(),
         inodeIter->getID());

      if ( tmpIter == modifiedIDs.end() )
      {
         modifiedIDs.push_back(inodeIter->getID());
         // set new numLinks
         inodeIter->setNumHardLinks(inodeIter->getNumHardLinks()*10);
         i++;
      }

      inodeIter++;
   }

   // last one is bad, but changed on-line
   inodeIter->setSize(inodeIter->getSize() + 1);
   this->db->getModificationEventsTable()->insert(
      FsckModificationEventList(1,
         FsckModificationEvent(ModificationEvent_FILEMOVED, inodeIter->getID() ) ),
      this->db->getModificationEventsTable()->newBulkHandle() );

   this->db->getDirInodesTable()->insert(inodes);
   this->db->getDentryTable()->insert(dentries);

   ASSERT_EQ(countCursor(this->db->findWrongInodeDirAttribs()), WRONG_DATA_INODES);
}

DB_TEST(testCheckForAndInsertFilesWithMissingStripeTargets)
{
   // we need a TargetMapper and a MirrorBuddyGroupMapper to test this
   TargetMapper targetMapper;
   MirrorBuddyGroupMapper buddyGroupMapper(&targetMapper);
   FsckTargetIDList usedTargets, goodTargets;
   FsckTargetIDList usedGroups, goodGroups;

   targetMapper.mapTarget(1, NumNodeID(1), StoragePoolStore::INVALID_POOL_ID);
   targetMapper.mapTarget(2, NumNodeID(1), StoragePoolStore::INVALID_POOL_ID);
   buddyGroupMapper.mapMirrorBuddyGroup(3, 1, 2, NumNodeID(0), false, NULL);

   goodTargets.push_back(FsckTargetID(1, FsckTargetIDType_TARGET) );
   goodTargets.push_back(FsckTargetID(2, FsckTargetIDType_TARGET) );
   goodGroups.push_back(FsckTargetID(3, FsckTargetIDType_BUDDYGROUP) );

   usedTargets.insert(usedTargets.end(), goodTargets.begin(), goodTargets.end() );
   usedTargets.push_back(FsckTargetID(4, FsckTargetIDType_TARGET) );

   usedGroups.insert(usedGroups.end(), goodGroups.begin(), goodGroups.end() );
   usedGroups.push_back(FsckTargetID(5, FsckTargetIDType_BUDDYGROUP) );

   // insert the used targets
   this->db->getUsedTargetIDsTable()->insert(usedTargets,
      this->db->getUsedTargetIDsTable()->newBulkHandle() );
   this->db->getUsedTargetIDsTable()->insert(usedGroups,
      this->db->getUsedTargetIDsTable()->newBulkHandle() );

   // create some files {good, bad} x {plain, mirrored}, plus a bad one that has changed on-line
   FsckFileInodeList inodes;
   DatabaseTk::createDummyFsckFileInodes(5, &inodes);

   FsckTargetIDList* patterns[5] = { &goodTargets, &goodGroups, &usedTargets, &usedGroups,
      &usedTargets };

   FsckDirEntryList dentries;

   unsigned i = 0;
   for (FsckFileInodeListIter file = inodes.begin(); file != inodes.end(); file++, i++)
   {
      FsckTargetIDList* pattern = patterns[i];
      UInt16Vector stripeTargets;

      for(FsckTargetIDListIter it = pattern->begin(); it != pattern->end(); ++it)
         stripeTargets.push_back(it->getID() );

      file->stripeTargets = stripeTargets;
      file->stripePatternType = pattern->front().getTargetIDType() == FsckTargetIDType_BUDDYGROUP
         ? FsckStripePatternType_BUDDYMIRROR
         : FsckStripePatternType_RAID0;

      dentries.push_back(
         FsckDirEntry(file->getID(), "file_" + file->getID(), file->getParentDirID(),
            file->getSaveNodeID(), file->getSaveNodeID(), FsckDirEntryType_REGULARFILE, false,
            file->getSaveNodeID(), 1, 1, false) );

      if(i == 4)
         this->db->getModificationEventsTable()->insert(
            FsckModificationEventList(1,
               FsckModificationEvent(ModificationEvent_FILEMOVED, file->getID() ) ),
      this->db->getModificationEventsTable()->newBulkHandle() );
   }

   this->db->getFileInodesTable()->insert(inodes);
   this->db->getDentryTable()->insert(dentries);

   FsckDirEntryList badFiles;
   drainToList(this->db->findFilesWithMissingStripeTargets(&targetMapper,
      &buddyGroupMapper), badFiles);

   ASSERT_EQ(badFiles.size(), 3u);
}

DB_TEST(testCheckForAndInsertChunksWithWrongPermissions)
{
   unsigned NUMBER_CHUNKS = 500;
   unsigned DIFF_UID = 50;
   unsigned DIFF_GID = 50;
   unsigned DIFF_TOTAL = DIFF_UID + DIFF_GID;

   FsckFileInodeList fileInodes;
   DatabaseTk::createDummyFsckFileInodes(NUMBER_CHUNKS, &fileInodes);

   FsckChunkList chunks;
   DatabaseTk::createDummyFsckChunks(NUMBER_CHUNKS, &chunks);

   FsckFileInodeListIter iter = fileInodes.begin();

   for (unsigned i = 0; i < DIFF_UID; i++)
   {
      iter->userID = 1000;
      iter++;
   }

   for (unsigned i = 0; i < DIFF_GID; i++)
   {
      iter->groupID = 1000;
      iter++;
   }

   iter->userID = 1;
   this->db->getModificationEventsTable()->insert(
      FsckModificationEventList(1,
         FsckModificationEvent(ModificationEvent_FILEMOVED, iter->getID() ) ),
      this->db->getModificationEventsTable()->newBulkHandle() );
   ++iter;

   //rest is owned by root
   while (iter != fileInodes.end())
   {
      iter->userID = 0;
      iter->groupID = 0;
      iter++;
   }

   this->db->getFileInodesTable()->insert(fileInodes);
   this->db->getChunksTable()->insert(chunks);

   ASSERT_EQ(countCursor(this->db->findChunksWithWrongPermissions()), DIFF_TOTAL);
}

DB_TEST(testCheckForAndInsertChunksInWrongPath)
{
   unsigned NUM_TARGETS = 2;
   unsigned NUM_CHUNKS_PER_TARGET = 5;
   unsigned NUM_WRONG_PATHS = 3;
   uint16_t nodeID = 1;

   FsckFileInodeList fileInodeList;
   FsckChunkList chunkList;

   for (unsigned i = 0; i < NUM_CHUNKS_PER_TARGET; i++)
   {
      std::string fileID = StorageTk::generateFileID(NumNodeID(nodeID));
      std::string parentDirID = StorageTk::generateFileID(NumNodeID(nodeID));

      // create a file inode for that
      unsigned origParentUID = i;
      PathInfo pathInfo(origParentUID, parentDirID, PATHINFO_FEATURE_ORIG);

      FsckFileInode fileInode = DatabaseTk::createDummyFsckFileInode();

      fileInode.id = fileID;
      fileInode.parentDirID = parentDirID;
      fileInode.parentNodeID = NumNodeID(nodeID);
      fileInode.pathInfo = pathInfo;
      UInt16Vector stripeTargets;
      for (unsigned j = 1;j <= NUM_TARGETS; j++)
         stripeTargets.push_back(j);
      fileInode.stripeTargets = stripeTargets;

      fileInodeList.push_back(fileInode);

      // create chunks
      for (unsigned j = 1;j <= NUM_TARGETS; j++)
      {
         FsckChunk chunk = DatabaseTk::createDummyFsckChunk();
         chunk.id = fileID;
         chunk.targetID = j;
         Path chunkDirPath;
         std::string chunkFilePath; // will be ignored
         StorageTk::getChunkDirChunkFilePath(&pathInfo, fileID, true, chunkDirPath, chunkFilePath);

         chunk.savedPath = Path(chunkDirPath.str());

         chunkList.push_back(chunk);
      }
   }

   // take the first NUM_WRONG_PATHS in the chunkList and set them to something stupid
   FsckChunkListIter iter = chunkList.begin();
   for (unsigned i=0; i<NUM_WRONG_PATHS; i++)
   {
      ASSERT_NE(iter, chunkList.end()) << "Programming error in test!";
      iter->savedPath = Path("/this/path/is/wrong");
      iter++;
   }

   chunkList.rbegin()->savedPath = Path("/wrong");
   this->db->getModificationEventsTable()->insert(
      FsckModificationEventList(1,
         FsckModificationEvent(ModificationEvent_FILEREMOVED, chunkList.rbegin()->getID() ) ),
      this->db->getModificationEventsTable()->newBulkHandle() );

   this->db->getFileInodesTable()->insert(fileInodeList);
   this->db->getChunksTable()->insert(chunkList);

   ASSERT_EQ(countCursor(this->db->findChunksInWrongPath()), NUM_WRONG_PATHS);
}

DB_TEST(testDeleteDentries)
{
   unsigned NUM_ELEMENTS = 10;
   unsigned NUM_DELETES = 4;

   // create some dentries
   FsckDirEntryList dentries;
   DatabaseTk::createDummyFsckDirEntries(NUM_ELEMENTS, &dentries);

   // insert them into the DB
   this->db->getDentryTable()->insert(dentries);

   // take the first NUM_DELETES and delete them in DB
   FsckDirEntryList dentriesDelete;
   FsckDirEntryListIter dentriesIter = dentries.begin();

   for (unsigned i = 0; i < NUM_DELETES; i++)
   {
      if (dentriesIter != dentries.end())
      {
         dentriesDelete.push_back(*dentriesIter);
         dentriesIter++;
      }
   }

   this->db->getDentryTable()->remove(dentriesDelete);

   FsckDirEntryList table = drain<FsckDirEntry>(this->db->getDentryTable()->get() );

   ASSERT_EQ(table.size(), NUM_ELEMENTS - NUM_DELETES);
}

DB_TEST(testDeleteChunks)
{
   unsigned NUM_ELEMENTS = 10;
   unsigned NUM_DELETES = 4;

   // create some dentries
   FsckChunkList chunks;
   DatabaseTk::createDummyFsckChunks(NUM_ELEMENTS, &chunks);

   //insert them into the DB
   this->db->getChunksTable()->insert(chunks);

   // take the first NUM_DELETES and delete them in DB
   FsckChunkListIter chunksIter = chunks.begin();

   for (unsigned i = 0; i < NUM_DELETES; i++, chunksIter++)
      this->db->getChunksTable()->remove(db::EntryID::fromStr(chunksIter->getID() ) );

   size_t count = countCursor(this->db->getChunksTable()->get() );

   ASSERT_EQ(count, NUM_ELEMENTS - NUM_DELETES);
}

DB_TEST(testDeleteFsIDs)
{
   unsigned NUM_ELEMENTS = 10;
   unsigned NUM_DELETES = 4;

   // create some fsIDs
   FsckFsIDList fsIDs;
   DatabaseTk::createDummyFsckFsIDs(NUM_ELEMENTS, &fsIDs);

   //insert them into the DB
   this->db->getFsIDsTable()->insert(fsIDs);

   // take the first NUM_DELETES and delete them in DB
   FsckFsIDList elemsDelete;
   FsckFsIDListIter iter = fsIDs.begin();

   for (unsigned i = 0; i < NUM_DELETES; i++)
   {
      if (iter != fsIDs.end())
      {
         elemsDelete.push_back(*iter);
         iter++;
      }
   }

   this->db->getFsIDsTable()->remove(elemsDelete);

   FsckFsIDList table = drain<FsckFsID>(this->db->getFsIDsTable()->get() );

   ASSERT_EQ(table.size(), NUM_ELEMENTS - NUM_DELETES);
}

DB_TEST(testGetFullPath)
{
   // first test => PASS OK
   FsckDirEntryList dirEntries;

   const char* const id_A = "1-1-1";
   const char* const id_B = "2-1-1";
   const char* const id_C = "3-1-1";
   const char* const id_D = "4-1-1";
   const char* const fileID = "5-1-1";

   FsckDirEntry fileDirEntry = DatabaseTk::createDummyFsckDirEntry();
   fileDirEntry.setName("file");
   fileDirEntry.setID(fileID);
   fileDirEntry.setParentDirID(META_ROOTDIR_ID_STR);
   dirEntries.push_back(fileDirEntry);

   this->db->getDentryTable()->insert(dirEntries);

   ASSERT_EQ(
      this->db->getDentryTable()->getPathOf(db::EntryID::fromStr(fileDirEntry.getID())),
      "/file");

   // second test => PASS OK
   this->db->getDentryTable()->clear();

   dirEntries.clear();

   fileDirEntry.setName("file");
   fileDirEntry.setID(fileID);
   fileDirEntry.setParentDirID(id_D);
   dirEntries.push_back(fileDirEntry);

   FsckDirEntry dirEntry = DatabaseTk::createDummyFsckDirEntry(FsckDirEntryType_DIRECTORY);

   dirEntry.setName("D");
   dirEntry.setID(id_D);
   dirEntry.setParentDirID(id_C);
   dirEntries.push_back(dirEntry);

   dirEntry.setName("C");
   dirEntry.setID(id_C);
   dirEntry.setParentDirID(id_B);
   dirEntries.push_back(dirEntry);

   dirEntry.setName("B");
   dirEntry.setID(id_B);
   dirEntry.setParentDirID(id_A);
   dirEntries.push_back(dirEntry);

   dirEntry.setName("A");
   dirEntry.setID(id_A);
   dirEntry.setParentDirID(META_ROOTDIR_ID_STR);
   dirEntries.push_back(dirEntry);

   this->db->getDentryTable()->insert(dirEntries);

   ASSERT_EQ(
      this->db->getDentryTable()->getPathOf(db::EntryID::fromStr(fileDirEntry.getID())),
      "/A/B/C/D/file");

   //==========//
   // third test => create a path which could not be fully resolved
   //=========//
   this->db->getDentryTable()->clear();

   dirEntries.clear();

   dirEntries.push_back(fileDirEntry);

   dirEntry.setName("D");
   dirEntry.setID(id_D);
   dirEntry.setParentDirID(id_C);
   dirEntries.push_back(dirEntry);

   dirEntry.setName("C");
   dirEntry.setID(id_C);
   dirEntry.setParentDirID(id_B);
   dirEntries.push_back(dirEntry);

   dirEntry.setName("A");
   dirEntry.setID(id_A);
   dirEntry.setParentDirID(META_ROOTDIR_ID_STR);
   dirEntries.push_back(dirEntry); 
   this->db->getDentryTable()->insert(dirEntries);

   ASSERT_EQ(
      this->db->getDentryTable()->getPathOf(db::EntryID::fromStr(fileDirEntry.getID())),
      "[<unresolved>]/C/D/file");

   //==========//
   // fourth test => create a loop
   //=========//
   this->db->getDentryTable()->clear();

   dirEntries.clear();

   dirEntries.push_back(fileDirEntry);

   dirEntry.setName("D");
   dirEntry.setID(id_D);
   dirEntry.setParentDirID(id_C);
   dirEntries.push_back(dirEntry);

   dirEntry.setName("C");
   dirEntry.setID(id_C);
   dirEntry.setParentDirID(id_B);
   dirEntries.push_back(dirEntry);

   dirEntry.setName("B");
   dirEntry.setID(id_B);
   dirEntry.setParentDirID(id_A);
   dirEntries.push_back(dirEntry);

   dirEntry.setName("A");
   dirEntry.setID(id_A);
   dirEntry.setParentDirID(id_D);
   dirEntries.push_back(dirEntry);

   this->db->getDentryTable()->insert(dirEntries);

   ASSERT_NE(
      this->db->getDentryTable()->getPathOf(db::EntryID::fromStr(fileDirEntry.getID())),
      "[<unresolved>]");
}

