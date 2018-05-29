#ifndef MODEHELP_H_
#define MODEHELP_H_

#include <app/config/Config.h>

#include "Mode.h"


class ModeHelp : public Mode
{
   public:
      ModeHelp(bool printAllModes = false):
         Mode(true), printAllModes(printAllModes)
      {
      }

      virtual int execute();

   private:
      bool printAllModes;

      void printGeneralHelp();
      void printSpecificHelp(const RunModesElem& runMode);
      void printSpecificHelpHeader();
};


#endif /*MODEHELP_H_*/
