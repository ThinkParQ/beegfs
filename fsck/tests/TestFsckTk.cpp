#include <common/toolkit/FsckTk.h>
#include <common/toolkit/StringTk.h>
#include <toolkit/DatabaseTk.h>
#include <toolkit/FsckTkEx.h>

#include <gtest/gtest.h>

TEST(FsckTk, dirEntryTypeConversion)
{
   FsckDirEntryType entryTypeOut;

   entryTypeOut = FsckTk::DirEntryTypeToFsckDirEntryType(DirEntryType_INVALID);
   ASSERT_EQ(entryTypeOut, FsckDirEntryType_INVALID);

   entryTypeOut = FsckTk::DirEntryTypeToFsckDirEntryType(DirEntryType_REGULARFILE);
   ASSERT_EQ(entryTypeOut, FsckDirEntryType_REGULARFILE);

   entryTypeOut = FsckTk::DirEntryTypeToFsckDirEntryType(DirEntryType_SYMLINK);
   ASSERT_EQ(entryTypeOut, FsckDirEntryType_SYMLINK);

   entryTypeOut = FsckTk::DirEntryTypeToFsckDirEntryType(DirEntryType_DIRECTORY);
   ASSERT_EQ(entryTypeOut, FsckDirEntryType_DIRECTORY);

   entryTypeOut = FsckTk::DirEntryTypeToFsckDirEntryType(DirEntryType_BLOCKDEV);
   ASSERT_EQ(entryTypeOut, FsckDirEntryType_SPECIAL);

   entryTypeOut = FsckTk::DirEntryTypeToFsckDirEntryType(DirEntryType_CHARDEV);
   ASSERT_EQ(entryTypeOut, FsckDirEntryType_SPECIAL);

   entryTypeOut = FsckTk::DirEntryTypeToFsckDirEntryType(DirEntryType_FIFO);
   ASSERT_EQ(entryTypeOut, FsckDirEntryType_SPECIAL);

   entryTypeOut = FsckTk::DirEntryTypeToFsckDirEntryType(DirEntryType_SOCKET);
   ASSERT_EQ(entryTypeOut, FsckDirEntryType_SPECIAL);
}
