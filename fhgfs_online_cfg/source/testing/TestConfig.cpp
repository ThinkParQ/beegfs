#include "TestConfig.h"

TestConfig::TestConfig()
   : log("TestConfig")
{
}

TestConfig::~TestConfig()
{
}

void TestConfig::setUp()
{
   emptyConfigFile = DUMMY_EMPTY_CONFIG_FILE;
   this->dummyConfigFile = DUMMY_NOEXIST_CONFIG_FILE;
}

void TestConfig::tearDown()
{
   // delete generated config file
   if (StorageTk::pathExists(this->emptyConfigFile))
   {
      /* TODO : return value of remove is ignored now;
       * maybe we should notify the user here (but that
       * would break test output)
       */
      remove(this->emptyConfigFile.c_str());
   }
}

void TestConfig::testMissingConfigFile()
{
   log.log(Log_DEBUG, "testMissingConfigFile started");

   // generate a bogus name for a config file
   /* normally the default file should be nonexistant, but to be absolutely sure,
    we check that and, in case, append a number */
   int appendix = 0;
   while (StorageTk::pathExists(this->dummyConfigFile))
   {
      appendix++;
      this->dummyConfigFile = DUMMY_NOEXIST_CONFIG_FILE + StringTk::intToStr(appendix);
   }

   int argc = 2;
   char* argv[2];

   std::string appNameStr = APP_NAME;
   char appName[appNameStr.length() + 1];
   strcpy(appName, appNameStr.c_str());

   std::string cfgLineStr = "cfgFile=" + this->dummyConfigFile;
   char cfgLine[cfgLineStr.length() + 1];
   strcpy(cfgLine, cfgLineStr.c_str());

   argv[0] = appName;
   argv[1] = cfgLine;

   // should throw InvalidConfigException now
   CPPUNIT_ASSERT_THROW(Config config(argc, argv), InvalidConfigException);

   log.log(Log_DEBUG, "testMissingConfigFile finished");
}
