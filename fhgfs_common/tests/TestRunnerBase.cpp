#include "include/tests/TestRunnerBase.h"

#include <cppunit/extensions/TestFactoryRegistry.h>

static AutomaticTestSuite* suites;

AutomaticTestSuite::AutomaticTestSuite(CppUnit::TestSuite* suite)
   : suite(suite), nextInList(suites)
{
   suites = this;
}

/*
 * @param outputFormat the output format of the tests (text,xml,...)
 *
 * no output file is given, so output goes to stderr
 */
TestRunnerBase::TestRunnerBase(TestRunnerOutputFormat outputFormat)
{
   this->outputFile = "";
   initOutputStream();
   this->outputFormat = outputFormat;
}

/*
 * @param outFile a filename to write the test outputs to (output will be appended)
 * @param outputFormat the output format of the tests (text,xml,...)
 *
 * @throw ComponentInitException
 */
TestRunnerBase::TestRunnerBase(std::string outputFile, TestRunnerOutputFormat outputFormat)
{
   this->outputFile = outputFile;
   initOutputStream();
   this->outputFormat = outputFormat;
}

TestRunnerBase::~TestRunnerBase()
{
   SAFE_DELETE(outputStream);
}

void TestRunnerBase::initOutputStream()
{
   if(!this->outputFile.empty())
   {
      this->outputStream = new std::ofstream(this->outputFile.c_str(), std::ios::app);
      if(this->outputStream->fail())
         throw ComponentInitException("Unable to open file for unit test output");
   }
   else
      this->outputStream = new std::ostream(std::cerr.rdbuf());
}

bool TestRunnerBase::run()
{
   if(this->addTests())
   {
      this->runTests();
      this->printOutput(this->outputFormat);

      return testResultCollector.testFailuresTotal() == 0;
   }

   return false;
}

bool TestRunnerBase::addTests()
{
   for (auto* suite = suites; suite; suite = suite->nextInList)
      testRunner.addTest(suite->suite);

   return true;
}

void TestRunnerBase::runTests()
{
   // Add the listener that collects test result
   this->testController.addListener(&(this->testResultCollector));

   // Add the listener that print dots as test run,
   // but only if we are not writing to a file
   if(this->outputFile.empty())
      this->testController.addListener(&this->progressListener);

   // run the tests
   this->testRunner.run(this->testController, "");
}

void TestRunnerBase::printOutput(TestRunnerOutputFormat outputFormat)
{
   switch(outputFormat)
   {
      case TestRunnerOutputFormat_COMPILER:
         printCompilerOutput();
         break;
      case TestRunnerOutputFormat_TEXT:
         printTextOutput();
         break;
      case TestRunnerOutputFormat_XML:
         printXmlOutput();
         break;
   }
}

void TestRunnerBase::printCompilerOutput()
{
   // Print test in a compiler compatible format
   CppUnit::CompilerOutputter compilerOutputter(&this->testResultCollector, *(this->outputStream));
   compilerOutputter.write();
}

void TestRunnerBase::printTextOutput()
{
   // Print test in a text format.
   CppUnit::TextOutputter textOutputter(&this->testResultCollector, *(this->outputStream));
   textOutputter.write();
}

void TestRunnerBase::printXmlOutput()
{
   // Print test in a xml format.
   CppUnit::XmlOutputter xmlOutputter(&this->testResultCollector, *(this->outputStream));
   xmlOutputter.write();
}

