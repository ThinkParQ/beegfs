#ifndef TESTRUNNER_H_
#define TESTRUNNER_H_

#include <common/testing/TestRunnerBase.h>

class TestRunner: public TestRunnerBase
{
   public:
      TestRunner(TestRunnerOutputFormat outputFormat);
      TestRunner(std::string outputFile, TestRunnerOutputFormat outputFormat);
      virtual ~TestRunner();

   protected:
      /*
       * return value indicates if any tests were added
       * (true if tests were added, false otherwise)
       */
      virtual bool addTests();
};

#endif /* TESTRUNNER_H_ */
