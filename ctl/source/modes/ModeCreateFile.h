#ifndef MODECREATEFILE_H_
#define MODECREATEFILE_H_

#include <common/storage/striping/StripePattern.h>
#include <common/Common.h>
#include "Mode.h"


class ModeCreateFile : public Mode
{
   public:
      virtual int execute();
      static void printHelp();
};

#endif /* MODECREATEFILE_H_ */
