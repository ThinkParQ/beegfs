#ifndef TESTDATABASE_H_
#define TESTDATABASE_H_

#include <common/net/message/NetMessage.h>
#include <database/FsckDB.h>

#include <boost/scoped_ptr.hpp>
#include <gtest/gtest.h>

class TestDatabase: public ::testing::Test
{
   public:
      TestDatabase();
      virtual ~TestDatabase();

      void SetUp();
      void TearDown();

   protected:
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
