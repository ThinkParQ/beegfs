#include "TestConfig.h"

#include <app/config/Config.h>

TestConfig::TestConfig()
   : log("TestConfig")
{
}

TestConfig::~TestConfig()
{
}

void TestConfig::SetUp()
{
   emptyConfigFile = DUMMY_EMPTY_CONFIG_FILE;
   this->dummyConfigFile = DUMMY_NOEXIST_CONFIG_FILE;
}

void TestConfig::TearDown()
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

TEST_F(TestConfig, missingConfigFile)
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
   appNameStr += '\0';

   std::string cfgLineStr = "cfgFile=" + this->dummyConfigFile;
   cfgLineStr += '\0';

   argv[0] = &appNameStr[0];
   argv[1] = &cfgLineStr[0];

   // should throw InvalidConfigException now
   ASSERT_THROW(Config config(argc, argv), InvalidConfigException);

   log.log(Log_DEBUG, "testMissingConfigFile finished");
}

TEST_F(TestConfig, defaultConfigFile)
{
   // get the path where the binary resides
   int BUFSIZE = 255;
   char exePathBuf[BUFSIZE];
   // read only BUFSIZE-1, as we need to terminate the string manually later
   ssize_t len = readlink("/proc/self/exe", exePathBuf, BUFSIZE-1);

   /* In case of an error, failure will indicate the error */
   if (len < 0)
      FAIL() << "Internal error";

   /* in case of insufficient buffer size, failure will indicate the error */
   if (len >= BUFSIZE)
      FAIL() << "Internal error";

   // readlink does NOT null terminate the string, so we do it here to be safe
   exePathBuf[len] = '\0';

   // construct the path to the default config file
   std::string defaultFileName = std::string(dirname(exePathBuf))
      + "/" + DEFAULT_CONFIG_FILE_RELATIVE;

   // create config with the default file and see what happens while parsing
   int argc = 2;
   char* argv[2];

   std::string appNameStr = APP_NAME;
   appNameStr += '\0';

   std::string cfgLineStr = "cfgFile=" + defaultFileName;
   cfgLineStr += '\0';

   argv[0] = &appNameStr[0];
   argv[1] = &cfgLineStr[0];

   try {
      Config config(argc, argv);
   } catch (ConnAuthFileException& e) {
      return;
   }
}
