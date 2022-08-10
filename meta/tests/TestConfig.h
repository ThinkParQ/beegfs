#ifndef TESTCONFIG_H_
#define TESTCONFIG_H_

#include <app/config/Config.h>
#include <common/app/log/LogContext.h>
#include <gtest/gtest.h>
#include <common/app/config/ConnAuthFileException.h>

#include <libgen.h>

#define DUMMY_NOEXIST_CONFIG_FILE      "/tmp/nonExistantConfigFile.conf.meta"
#define DUMMY_EMPTY_CONFIG_FILE        "/tmp/emptyConfigFile.conf.meta"
#define DEFAULT_CONFIG_FILE_RELATIVE   "dist/etc/beegfs-meta.conf"
#define APP_NAME                       "beegfs-meta";

class TestConfig: public ::testing::Test
{
   public:
      TestConfig();
      virtual ~TestConfig();

      void SetUp() override;
      void TearDown() override;

   protected:
      LogContext log;

      // we have these filenames as member variables because
      // we might need to delete them in tearDown function
      std::string dummyConfigFile;
      std::string emptyConfigFile;
};

#endif /* TESTCONFIG_H_ */
