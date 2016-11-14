#ifndef TESTCONFIG_H_
#define TESTCONFIG_H_

#include <app/config/Config.h>
#include <common/app/log/LogContext.h>
#include <cppunit/TestAssert.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <libgen.h>

#define DUMMY_NOEXIST_CONFIG_FILE      "/tmp/nonExistantConfigFile.conf.admon"
#define DUMMY_EMPTY_CONFIG_FILE        "/tmp/emptyConfigFile.conf.admon"
#define DEFAULT_CONFIG_FILE_RELATIVE   "dist/etc/beegfs-admon.conf"
#define APP_NAME                       "beegfs-admon";

class TestConfig: public CppUnit::TestFixture
{
   CPPUNIT_TEST_SUITE( TestConfig );

   CPPUNIT_TEST( testMissingConfigFile );
   CPPUNIT_TEST( testDefaultConfigFile );

   CPPUNIT_TEST_SUITE_END();

   public:
      TestConfig();
      virtual ~TestConfig();

      void setUp();
      void tearDown();

      void testMissingConfigFile();
      void testDefaultConfigFile();

   private:
      LogContext log;
      // we have these filenames as member variables because
      // we might need to delete them in tearDown function
      std::string dummyConfigFile;
      std::string emptyConfigFile;
};

#endif /* TESTCONFIG_H_ */
