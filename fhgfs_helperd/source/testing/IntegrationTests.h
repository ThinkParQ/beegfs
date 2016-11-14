#ifndef INTEGRATIONTESTS_H_
#define INTEGRATIONTESTS_H_

#include "common/Common.h"

class IntegrationTests
{
   public:
      static bool perform();
   
   private:
      IntegrationTests() {}
      
      static bool testStreamWorkers();
};

#endif /*INTEGRATIONTESTS_H_*/
