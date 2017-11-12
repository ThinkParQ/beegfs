#ifndef MODEHELP_H_
#define MODEHELP_H_

#include <app/config/Config.h>
#include <common/Common.h>

#include "Mode.h"


class ModeHelp : public Mode
{
   public:
      ModeHelp() {};

      virtual int execute();

   private:
      void printGeneralHelp();
      void printSpecificHelp(RunMode runMode);
      void printSpecificHelpHeader();
};


#endif /*MODEHELP_H_*/
