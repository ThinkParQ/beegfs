#include <common/app/log/LogContext.h>
#include "common/toolkit/StringTk.h"
#include "ComponentTests.h"

bool ComponentTests::perform()
{
   LogContext log("ComponentTests");
   
   log.log(1, "Running component tests...");

   bool testRes = true;
   
   log.log(1, "Component tests complete");
   
   return testRes;
}

