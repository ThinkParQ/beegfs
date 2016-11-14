#include "TestRunner.h"

#include "TestConfig.h"
#include "TestCursors.h"
#include "TestDatabase.h"
#include "TestFsckTk.h"
#include "TestMsgSerialization.h"
#include "TestSerialization.h"
#include "TestSetFragment.h"
#include "TestSet.h"
#include "TestTable.h"

#include <program/Program.h>

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
      this->testRunner.addTest(TestCursors::suite());
      this->testRunner.addTest(TestSerialization::suite());
      this->testRunner.addTest(TestMsgSerialization::suite());
      this->testRunner.addTest(TestSetFragment::suite());
      this->testRunner.addTest(TestSet::suite());
      this->testRunner.addTest(TestTable::suite());
      this->testRunner.addTest(TestDatabase::suite());
      this->testRunner.addTest(TestFsckTk::suite());
      return true;
   }
   else
      return false;
}

