#ifndef MODE_H_
#define MODE_H_

#include <common/nodes/Node.h>
#include <common/Common.h>

class Mode
{
   public:
      virtual ~Mode() {}

      /**
       * Executes the mode inside the calling thread.
       *
       * @return APPCODE_...
       */
      virtual int execute() = 0;

   protected:
      Mode() {}
      bool checkInvalidArgs(const StringMap* cfg);
};

#endif /*MODE_H_*/
