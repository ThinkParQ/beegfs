#include <common/toolkit/LockFD.h>
#include <common/toolkit/StorageTk.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>

#include <gtest/gtest.h>

class TestLockFD : public ::testing::Test {
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

TEST_F(TestLockFD, testInitialLock)
{
   const auto path = tmpDir + "/initial";

   auto lock = LockFD::lock(path, false);
   ASSERT_TRUE(lock);

   FDHandle fd(open(path.c_str(), O_RDONLY));
   ASSERT_TRUE(fd.valid());

   ASSERT_EQ(flock(*fd, LOCK_EX | LOCK_NB), -1);
   ASSERT_EQ(errno, EWOULDBLOCK);
}

TEST_F(TestLockFD, testLockTwice)
{
   const auto path = tmpDir + "/twice";

   auto lock1 = LockFD::lock(path, false);
   ASSERT_TRUE(lock1);

   auto lock2 = LockFD::lock(path, false);
   ASSERT_FALSE(lock2);
   ASSERT_EQ(lock2.error().value(), EWOULDBLOCK);
}

TEST_F(TestLockFD, testDoesUnlink)
{
   const auto path = tmpDir + "/unlinks";

   auto lock = LockFD::lock(path, false);
   ASSERT_TRUE(lock);
   lock = {};

   ASSERT_EQ(access(path.c_str(), R_OK), -1);
}

TEST_F(TestLockFD, testUpdate)
{
   const auto path = tmpDir + "/update";

   {
      auto lock = LockFD::lock(path, false).release_value();
      ASSERT_TRUE(lock.update("test"));
      ASSERT_TRUE(lock.updateWithPID());
   }

   {
      auto lock = LockFD::lock(path, true).release_value();

      {
         ASSERT_FALSE(lock.update("test"));
         std::ifstream file(path);
         char buf[10];
         file.read(buf, 5);
         ASSERT_TRUE(file.eof());
         ASSERT_EQ(file.gcount(), 4);
         ASSERT_EQ(memcmp(buf, "test", 4), 0);
      }

      {
         ASSERT_FALSE(lock.updateWithPID());
         std::ifstream file(path);
         char buf[100];
         const auto expected = std::to_string(getpid()) + '\n';
         file.read(buf, 100);
         ASSERT_TRUE(file.eof());
         ASSERT_EQ(size_t(file.gcount()), expected.size());
         ASSERT_EQ(memcmp(buf, &expected[0], expected.size()), 0);
      }
   }
}
