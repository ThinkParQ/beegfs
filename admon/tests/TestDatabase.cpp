#include <common/toolkit/StorageTk.h>
#include <components/Database.h>

#include <gtest/gtest.h>

#define DB_TEST_FILE "/tmp/beegfs_admon/testing/db/test.db"

class TestDatabase : public ::testing::Test {
   protected:
      std::string testDbFile;

      void SetUp() override
      {
         this->testDbFile = DB_TEST_FILE;
      }

      void TearDown() override
      {
         // delete generated config file
         if (StorageTk::pathExists(this->testDbFile))
         {
            /* TODO : return value of remove is ignored now;
             * maybe we should notify the user here (but that
             * would break test output)
             */
            remove(this->testDbFile.c_str());
         }
      }
};

/*
 * create a new empty database
 */
TEST_F(TestDatabase, DISABLED_newDBCreation)
{
   Database db(this->testDbFile, false);
}

/*
 * create a new empty database, close it and reopen it
 */
TEST_F(TestDatabase, DISABLED_existingDBOpen)
{
   Database* dbA = new Database(this->testDbFile, false);
   SAFE_DELETE(dbA);
   Database* dbB = new Database(this->testDbFile, false);
   SAFE_DELETE(dbB);
}

/*
 * create a file with something (but definitely not readable by sqlite) and try to open it
 * a DatabaseException is expected to be thrown
 */
TEST_F(TestDatabase, DISABLED_brokenDBOpen)
{
   // create the dbFile and write some garbish into it
   std::ofstream dbFileStream(this->testDbFile.c_str());
   dbFileStream.put('x');
   dbFileStream.close();

   // now try to open that as DB
   Database db(this->testDbFile, false);

   // the desired behaviour here would be, that an exception is thrown, but at the moment this is not the case
   // => Database class must be changed
}

/*
 * create a file with something (but definitely not readable by sqlite) and try to open it
 * with the clearDatabase flag => should succeed, because file should be deleted
 */
TEST_F(TestDatabase, DISABLED_clearBrokenDBOpen)
{
   // create the dbFile and write some garbish into it
   std::ofstream dbFileStream(this->testDbFile.c_str());
   dbFileStream.put('x');
   dbFileStream.close();

   // now try to open that as DB
   Database db(this->testDbFile, true);
}
