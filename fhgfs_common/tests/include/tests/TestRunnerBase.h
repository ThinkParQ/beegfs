#ifndef TESTRUNNERBASE_H
#define TESTRUNNERBASE_H

#include <common/components/ComponentInitException.h>
#include <cppunit/TestResult.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TestRunner.h>
#include <cppunit/TestSuite.h>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/TextOutputter.h>
#include <cppunit/XmlOutputter.h>
#include <cppunit/TextTestProgressListener.h>
#include <cppunit/ui/text/TestRunner.h>

#include <fstream>
#include <iostream>

enum TestRunnerOutputFormat
{
   TestRunnerOutputFormat_COMPILER = 0,
   TestRunnerOutputFormat_TEXT = 1,
   TestRunnerOutputFormat_XML = 2
};

class TestRunnerBase
{
   public:
      TestRunnerBase(TestRunnerOutputFormat outputFormat);
      TestRunnerBase(std::string outputFile, TestRunnerOutputFormat outputFormat);
      virtual ~TestRunnerBase();

      bool run();

      void printOutput(TestRunnerOutputFormat outputFormat);

   protected:
      CppUnit::TextTestRunner testRunner;
      // the event manager and test controller
      CppUnit::TestResult testController;
      // a listener that collects test results
      CppUnit::TestResultCollector testResultCollector;
      // a listener that print dots as test run
      CppUnit::TextTestProgressListener progressListener;

      std::string outputFile;
      std::ostream* outputStream;
      TestRunnerOutputFormat outputFormat;

      void initOutputStream();

      /*
       * return value indicates if any tests were added
       * (true if tests were added, false otherwise)
       */
      virtual bool addTests();

      void printCompilerOutput();
      void printTextOutput();
      void printXmlOutput();

      void runTests();
};

struct AutomaticTestSuite
{
   CppUnit::TestSuite* suite;
   AutomaticTestSuite* nextInList;

   AutomaticTestSuite(CppUnit::TestSuite* suite);
};

#define REGISTER_TEST_SUITE_CONCAT(a, b) a ## b
#define REGISTER_TEST_SUITE(Suite) \
   static AutomaticTestSuite REGISTER_TEST_SUITE_CONCAT(_registerSuite__, __COUNTER__)(\
         Suite::suite())

#endif /* TESTRUNNERBASE_H */
