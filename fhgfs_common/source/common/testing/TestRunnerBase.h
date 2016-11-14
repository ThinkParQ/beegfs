#ifndef TESTRUNNERBASE_H
#define TESTRUNNERBASE_H

#include <common/threading/PThread.h>
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

/*
 * The TestRunner class basically doesn't have to be a seperate thread, but in the first design
 * it was derived from PThread to have to possibility to let it run in parallel to the normal
 * FhGFS functionality. We do not do this in the current implementation, but nevertheless we leave
 * TestRunner as PThread for now
 */
class TestRunnerBase : public PThread
{
   public:
      TestRunnerBase(TestRunnerOutputFormat outputFormat);
      TestRunnerBase(std::string outputFile, TestRunnerOutputFormat outputFormat);
      virtual ~TestRunnerBase();

      virtual void run();

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
      virtual bool addTests() = 0;
      void addCommonTests();

      void printCompilerOutput();
      void printTextOutput();
      void printXmlOutput();

      void runTests();
};

#endif /* TESTRUNNERBASE_H */
