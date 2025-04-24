#pragma once

#include "Work.h"

class DummyWork : public Work
{
   public:

   virtual void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
   {
      // nothing to be done here
   }
};

