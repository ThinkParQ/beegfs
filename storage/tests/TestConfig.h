#ifndef TESTCONFIG_H_
#define TESTCONFIG_H_

#include <app/config/Config.h>
#include <common/app/log/LogContext.h>
#include <gtest/gtest.h>
#include <common/app/config/ConnAuthFileException.h>

#include <libgen.h>

#define DUMMY_NOEXIST_CONFIG_FILE      "/tmp/nonExistantConfigFile.conf.storage"
#define DUMMY_EMPTY_CONFIG_FILE        "/tmp/emptyConfigFile.conf.storage"
#define DEFAULT_CONFIG_FILE_RELATIVE   "dist/etc/beegfs-storage.conf"
#define APP_NAME                       "beegfs-storage";

class TestConfig: public ::testing::Test
{
   public:
      TestConfig();
      virtual ~TestConfig();

      void SetUp() override;
      void TearDown() override;

   protected:
      // we have these filenames as member variables because
      // we might need to delete them in tearDown function
      std::string dummyConfigFile;
      std::string emptyConfigFile;
};

#endif /* TESTCONFIG_H_ */
