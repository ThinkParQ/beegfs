#ifndef FaultInject_h_fcBgRTb4sQ1jbpOon87vPS
#define FaultInject_h_fcBgRTb4sQ1jbpOon87vPS

#include <string>
#include "SafeFD.h"

class InjectFault {
   private:
      SafeFD faultDir;

   public:
      explicit InjectFault(const char* type, bool quiet = false);
      ~InjectFault();

      InjectFault(InjectFault&&) = delete;
      InjectFault& operator=(InjectFault&&) = delete;
};

#endif
