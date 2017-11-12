#ifndef SOURCE_COMPONENTS_HEALTHCHECKER_H_
#define SOURCE_COMPONENTS_HEALTHCHECKER_H_

#include <common/threading/PThread.h>



class HealthChecker: public PThread
{
   public:
      HealthChecker() : PThread("HealthChecker") {};
      virtual ~HealthChecker() {};


   private:
      virtual void run();
};

#endif /* SOURCE_COMPONENTS_HEALTHCHECKER_H_ */
