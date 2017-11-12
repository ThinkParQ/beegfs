#include "TestDatabase.h"

#include <common/toolkit/StorageTk.h>
#include <components/Database.h>


TestDatabase::TestDatabase()
   : log("TestDatabase")
{
}

TestDatabase::~TestDatabase()
{
}

void TestDatabase::setUp()
{
   this->testDbFile = DB_TEST_FILE;
}

void TestDatabase::tearDown()
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

/*
 * create a new empty database
 */
void TestDatabase::testNewDBCreation()
{
   log.log(Log_DEBUG, "testNewDBCreation started");

   Database db(this->testDbFile, false);

   log.log(Log_DEBUG, "testNewDBCreation finished");
}

/*
 * create a new empty database, close it and reopen it
 */
void TestDatabase::testExistingDBOpen()
{
   log.log(Log_DEBUG, "testExistingDBOpen started");

   Database* dbA = new Database(this->testDbFile, false);
   SAFE_DELETE(dbA);
   Database* dbB = new Database(this->testDbFile, false);
   SAFE_DELETE(dbB);

   log.log(Log_DEBUG, "testExistingDBOpen finished");
}

/*
 * create a file with something (but definitely not readable by sqlite) and try to open it
 * a DatabaseException is expected to be thrown
 */
void TestDatabase::testBrokenDBOpen()
{
   log.log(Log_DEBUG, "testBrokenDBOpen started");

   // create the dbFile and write some garbish into it
   std::ofstream dbFileStream(this->testDbFile.c_str());
   dbFileStream.put('x');
   dbFileStream.close();

   // now try to open that as DB
   Database db(this->testDbFile, false);

   // the desired behaviour here would be, that an exception is thrown, but at the moment this is not the case
   // => Database class must be changed
   log.log(Log_DEBUG, "testBrokenDBOpen finished");
}

/*
 * create a file with something (but definitely not readable by sqlite) and try to open it
 * with the clearDatabase flag => should succeed, because file should be deleted
 */
void TestDatabase::testClearBrokenDBOpen()
{
   log.log(Log_DEBUG, "testClearBrokenDBOpen started");

   // create the dbFile and write some garbish into it
   std::ofstream dbFileStream(this->testDbFile.c_str());
   dbFileStream.put('x');
   dbFileStream.close();

   // now try to open that as DB
   Database db(this->testDbFile, true);

   log.log(Log_DEBUG, "testClearBrokenDBOpen finished");
}
