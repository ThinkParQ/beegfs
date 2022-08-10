#ifndef TESTCONFIG_H_
#define TESTCONFIG_H_

#include <app/config/Config.h>

#include <libgen.h>
#include <gtest/gtest.h>
#include <common/app/config/ConnAuthFileException.h>

#define DUMMY_NOEXIST_CONFIG_FILE "/tmp/nonExistantConfigFile.fsck"
#define DUMMY_EMPTY_CONFIG_FILE "/tmp/emptyConfigFile.fsck"
#define DEFAULT_CONFIG_FILE_RELATIVE "dist/etc/beegfs-client.conf"
#define APP_NAME "beegfs-fsck";

class TestConfig: public ::testing::Test
{
   protected:
      void SetUp();
      void TearDown();

      LogContext log;

      // we have these filenames as member variables because
      // we might need to delete them in tearDown function
      std::string dummyConfigFile;
      std::string emptyConfigFile;
};

#endif /* TESTCONFIG_H_ */
