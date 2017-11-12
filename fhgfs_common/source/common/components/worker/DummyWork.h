#ifndef DUMMYWORK_H_
#define DUMMYWORK_H_

#include "Work.h"

class DummyWork : public Work
{
   public:

   virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
   {
      // nothing to be done here
   }
};

#endif /*DUMMYWORK_H_*/
