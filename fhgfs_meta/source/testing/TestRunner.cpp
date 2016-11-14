#include "TestRunner.h"

#include "TestCommunication.h"
#include "TestConfig.h"
#include "TestSerialization.h"
#include "TestMsgSerialization.h"

#include <common/testing/TestListTk.h>
#include <common/testing/TestRWLock.h>
#include <common/testing/TestUnitTk.h>
#include <common/testing/TestVersionTk.h>
#include <program/Program.h>

#include "TestBuddyMirroring.h"
/*
 * @param outputFormat the output format of the tests (text,xml,...)
 *
 * no output file is given, so output goes to stderr
 */
TestRunner::TestRunner(TestRunnerOutputFormat outputFormat) :
   TestRunnerBase(outputFormat)
{
}

/*
 * @param outFile a filename to write the test outputs to (output will be appended)
 * @param outputFormat the output format of the tests (text,xml,...)
 *
 * @throw ComponentInitException
 */
TestRunner::TestRunner(std::string outputFile, TestRunnerOutputFormat outputFormat) :
   TestRunnerBase(outputFile, outputFormat)
{
}

TestRunner::~TestRunner()
{
}

bool TestRunner::addTests()
{
   Config* cfg = Program::getApp()->getConfig();
   if(cfg->getRunUnitTests())
   {
      this->testRunner.addTest(TestConfig::suite());
      this->testRunner.addTest(TestSerialization::suite());
      this->testRunner.addTest(TestMsgSerialization::suite());
      //this->testRunner.addTest(TestCommunication::suite()); // test not working
      // this->testRunner.addTest(TestRWLock::suite()); // commented out because of long runtime
      this->testRunner.addTest(TestUnitTk::suite());
      this->testRunner.addTest(TestListTk::suite());
      this->testRunner.addTest(TestVersionTk::suite());
      this->testRunner.addTest(TestBuddyMirroring::suite());
      return true;
   }
   else
      return false;
}

