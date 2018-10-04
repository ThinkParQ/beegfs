#include <common/toolkit/PreallocatedFile.h>
#include <common/toolkit/StorageTk.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>

#include <gtest/gtest.h>

class TestPreallocatedFile : public ::testing::Test {
   protected:
      std::string tmpDir;

      void SetUp() override
      {
         tmpDir = "tmpXXXXXX";
         tmpDir += '\0';
         ASSERT_NE(mkdtemp(&tmpDir[0]), nullptr);
         tmpDir.resize(tmpDir.size() - 1);
      }

      void TearDown() override
      {
         StorageTk::removeDirRecursive(tmpDir);
      }
};

TEST_F(TestPreallocatedFile, allocation)
{
   const auto path = tmpDir + "/file";

   PreallocatedFile<char, 1024*1024> file(path, 0600);

   struct stat st;
   ASSERT_EQ(::stat(path.c_str(), &st), 0);
   ASSERT_EQ(st.st_size, 1024*1024+1);
   ASSERT_GE(st.st_blocks * 512, 1024*1024+1);
}

TEST_F(TestPreallocatedFile, allocationFailure)
{
   // This should throw because filesystems can't provide 2**63 writable bytes in a single file as
   // of writing this test and probably won't for a long time after.
   ASSERT_THROW(([this] {
            PreallocatedFile<char, std::numeric_limits<off_t>::max()> file(tmpDir + "/file", 0600);
         })(),
         std::system_error);
}

TEST_F(TestPreallocatedFile, readWrite)
{
   const auto goodPath = tmpDir + "/good";

   PreallocatedFile<uint64_t> good(goodPath, 0600);
   PreallocatedFile<uint64_t, 1> bad(tmpDir + "/bad", 0600);

   good.write(0x0102030405060708ULL);
   ASSERT_THROW(bad.write(0), std::runtime_error);

   ASSERT_EQ(*good.read(), 0x0102030405060708ULL);
   ASSERT_EQ(bad.read(), boost::none);

   ASSERT_EQ(::truncate(goodPath.c_str(), 0), 0);
   ASSERT_THROW(good.read(), std::system_error);
}
