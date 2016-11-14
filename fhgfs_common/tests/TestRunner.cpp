#include "TestRunner.h"

#include "TestSerialization.h"
#include "TestTimerQueue.h"
#include "common/testing/TestBitStore.h"

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
   this->testRunner.addTest(TestSerialization::suite() );
   this->testRunner.addTest(TestBitStore::suite() );
   this->testRunner.addTest(TestTimerQueue::suite());
   return true;
}

