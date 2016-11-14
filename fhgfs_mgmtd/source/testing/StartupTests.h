#ifndef STARTUPTESTS_H_
#define STARTUPTESTS_H_

#include "common/Common.h"

class StartupTests
{
   public:
      static bool perform();

   private:
      StartupTests() {}


      static bool testPath();
      static bool testLogging();
      static bool testNICsHwAddr();
      static bool testNICs();
};

#endif /*STARTUPTESTS_H_*/
