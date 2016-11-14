#include <common/app/log/LogContext.h>
#include "common/toolkit/StringTk.h"
#include "common/toolkit/MessagingTk.h"
#include <program/Program.h>
#include "IntegrationTests.h"

bool IntegrationTests::perform()
{
   LogContext log("IntegrationTests");
   
   log.log(1, "Running integration tests...");

   bool testRes = true;
   
   log.log(1, "Integration tests complete");
   
   return testRes;
}







