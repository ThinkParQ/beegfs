#ifndef TESTDATABASE_H_
#define TESTDATABASE_H_

#include <common/net/message/NetMessage.h>
#include <database/FsckDB.h>

#include <boost/scoped_ptr.hpp>
#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class TestDatabase: public CppUnit::TestFixture
{
   CPPUNIT_TEST_SUITE( TestDatabase );

   CPPUNIT_TEST(testInsertAndReadSingleDentry);

   CPPUNIT_TEST(testUpdateDentries);
   CPPUNIT_TEST(testUpdateDirInodes);
   CPPUNIT_TEST(testUpdateFileInodes);
   CPPUNIT_TEST(testUpdateChunks);

   CPPUNIT_TEST(testInsertAndReadSingleFileInode);

   CPPUNIT_TEST(testInsertAndReadSingleDirInode);
   CPPUNIT_TEST(testInsertAndReadDirInodes);

   CPPUNIT_TEST(testInsertAndReadSingleChunk);
   CPPUNIT_TEST(testInsertAndReadChunks);

   CPPUNIT_TEST(testInsertAndReadSingleContDir);
   CPPUNIT_TEST(testInsertAndReadContDirs);

   CPPUNIT_TEST(testInsertAndReadSingleFsID);
   CPPUNIT_TEST(testInsertAndReadFsIDs);

   CPPUNIT_TEST(testFindDuplicateInodeIDs);
   CPPUNIT_TEST(testFindDuplicateChunks);
   CPPUNIT_TEST(testFindDuplicateContDirs);
   CPPUNIT_TEST(testFindMismirroredDentries);
   CPPUNIT_TEST(testFindMismirroredDirectories);

   CPPUNIT_TEST(testCheckForAndInsertDanglingDirEntries);
   CPPUNIT_TEST(testCheckForAndInsertInodesWithWrongOwner);
   CPPUNIT_TEST(testCheckForAndInsertDirEntriesWithWrongOwner);
   CPPUNIT_TEST(testCheckForAndInsertMissingDentryByIDFile);
   CPPUNIT_TEST(testCheckForAndInsertOrphanedDentryByIDFiles);
   CPPUNIT_TEST(testCheckForAndInsertOrphanedDirInodes);
   CPPUNIT_TEST(testCheckForAndInsertOrphanedFileInodes);
   CPPUNIT_TEST(testCheckForAndInsertOrphanedChunks);
   CPPUNIT_TEST(testCheckForAndInsertInodesWithoutContDir);
   CPPUNIT_TEST(testCheckForAndInsertOrphanedContDirs);
   CPPUNIT_TEST(testCheckForAndInsertFileInodesWithWrongAttribs);
   CPPUNIT_TEST(testCheckForAndInsertDirInodesWithWrongAttribs);
   CPPUNIT_TEST(testCheckForAndInsertFilesWithMissingStripeTargets);
   CPPUNIT_TEST(testCheckForAndInsertChunksWithWrongPermissions);
   CPPUNIT_TEST(testCheckForAndInsertChunksInWrongPath);

   CPPUNIT_TEST(testDeleteDentries);
   CPPUNIT_TEST(testDeleteChunks);
   CPPUNIT_TEST(testDeleteFsIDs);

   CPPUNIT_TEST(testGetFullPath);

   CPPUNIT_TEST_SUITE_END();

   public:
      TestDatabase();
      virtual ~TestDatabase();

      void setUp();
      void tearDown();

   private:
      std::string databasePath;
      boost::scoped_ptr<FsckDB> db;

      void testInsertAndReadSingleDentry();
      void testInsertAndReadDentries();

      void testUpdateDentries();
      void testUpdateDirInodes();
      void testUpdateFileInodes();
      void testUpdateChunks();

      void testInsertAndReadSingleFileInode();

      void testInsertAndReadSingleDirInode();
      void testInsertAndReadDirInodes();

      void testInsertAndReadSingleChunk();
      void testInsertAndReadChunks();

      void testInsertAndReadSingleContDir();
      void testInsertAndReadContDirs();

      void testInsertAndReadSingleFsID();
      void testInsertAndReadFsIDs();

      void testFindDuplicateInodeIDs();
      void testFindDuplicateChunks();
      void testFindDuplicateContDirs();
      void testFindMismirroredDentries();
      void testFindMismirroredDirectories();

      void testCheckForAndInsertDanglingDirEntries();
      void testCheckForAndInsertInodesWithWrongOwner();
      void testCheckForAndInsertDirEntriesWithWrongOwner();
      void testCheckForAndInsertMissingDentryByIDFile();
      void testCheckForAndInsertOrphanedDentryByIDFiles();
      void testCheckForAndInsertOrphanedDirInodes();
      void testCheckForAndInsertOrphanedFileInodes();
      void testCheckForAndInsertOrphanedChunks();
      void testCheckForAndInsertInodesWithoutContDir();
      void testCheckForAndInsertOrphanedContDirs();
      void testCheckForAndInsertFileInodesWithWrongAttribs();
      void testCheckForAndInsertDirInodesWithWrongAttribs();
      void testCheckForAndInsertFilesWithMissingStripeTargets();
      void testCheckForAndInsertChunksWithWrongPermissions();
      void testCheckForAndInsertChunksInWrongPath();

      void testDeleteDentries();
      void testDeleteChunks();
      void testDeleteFsIDs();

      void testGetRowCount();

      void testGetFullPath();

      template<typename Table, typename Items>
      static bool setRepairActions(Table* table, std::list<Items>& items, FsckRepairAction action)
      {
         for (typename std::list<Items>::iterator it = items.begin(), end = items.end();
               it != end;
               ++it)
         {
            if (!table->setRepairAction(*it, action) )
               return false;
         }

         return true;
      }

      template<typename Item, typename Source>
      static void drainToList(Source source, std::list<Item>& list)
      {
         list.clear();
         while(source.step() )
            list.push_back(*source.get() );
      }

      template<typename Item, typename Source>
      static std::list<Item> drain(Source source)
      {
         std::list<Item> list;
         drainToList(source, list);
         return list;
      }

      template<typename Source>
      static size_t countCursor(Source source)
      {
         size_t result = 0;
         while(source.step() )
            result++;
         return result;
      }
};

#endif /* TESTDATABASE_H_ */
