#include "TestFsckTk.h"
#include <tests/TestRunnerBase.h>

#include <common/toolkit/FsckTk.h>
#include <common/toolkit/StringTk.h>
#include <toolkit/DatabaseTk.h>
#include <toolkit/FsckTkEx.h>

REGISTER_TEST_SUITE(TestFsckTk);

TestFsckTk::TestFsckTk()
{
}

TestFsckTk::~TestFsckTk()
{
}

void TestFsckTk::setUp()
{
}

void TestFsckTk::tearDown()
{
}

void TestFsckTk::testDirEntryTypeConversion()
{
   FsckDirEntryType entryTypeOut;

   entryTypeOut = FsckTk::DirEntryTypeToFsckDirEntryType(DirEntryType_INVALID);
   CPPUNIT_ASSERT_MESSAGE("DirEntryType_INVALID does not convert correctly",
      entryTypeOut == FsckDirEntryType_INVALID);

   entryTypeOut = FsckTk::DirEntryTypeToFsckDirEntryType(DirEntryType_REGULARFILE);
   CPPUNIT_ASSERT_MESSAGE("DirEntryType_REGULARFILE does not convert correctly",
      entryTypeOut == FsckDirEntryType_REGULARFILE);

   entryTypeOut = FsckTk::DirEntryTypeToFsckDirEntryType(DirEntryType_SYMLINK);
   CPPUNIT_ASSERT_MESSAGE("DirEntryType_SYMLINK does not convert correctly",
      entryTypeOut == FsckDirEntryType_SYMLINK);

   entryTypeOut = FsckTk::DirEntryTypeToFsckDirEntryType(DirEntryType_DIRECTORY);
   CPPUNIT_ASSERT_MESSAGE("DirEntryType_DIRECTORY does not convert correctly",
      entryTypeOut == FsckDirEntryType_DIRECTORY);

   entryTypeOut = FsckTk::DirEntryTypeToFsckDirEntryType(DirEntryType_BLOCKDEV);
   CPPUNIT_ASSERT_MESSAGE("DirEntryType_BLOCKDEV does not convert correctly",
      entryTypeOut == FsckDirEntryType_SPECIAL);

   entryTypeOut = FsckTk::DirEntryTypeToFsckDirEntryType(DirEntryType_CHARDEV);
   CPPUNIT_ASSERT_MESSAGE("DirEntryType_CHARDEV does not convert correctly",
      entryTypeOut == FsckDirEntryType_SPECIAL);

   entryTypeOut = FsckTk::DirEntryTypeToFsckDirEntryType(DirEntryType_FIFO);
   CPPUNIT_ASSERT_MESSAGE("DirEntryType_FIFO does not convert correctly",
      entryTypeOut == FsckDirEntryType_SPECIAL);

   entryTypeOut = FsckTk::DirEntryTypeToFsckDirEntryType(DirEntryType_SOCKET);
   CPPUNIT_ASSERT_MESSAGE("DirEntryType_SOCKET does not convert correctly",
      entryTypeOut == FsckDirEntryType_SPECIAL);
}
