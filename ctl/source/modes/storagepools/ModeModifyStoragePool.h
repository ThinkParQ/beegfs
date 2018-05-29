#ifndef CTL_MODEMODIFYSTORAGEPOOL_H_
#define CTL_MODEMODIFYSTORAGEPOOL_H_

#include <modes/Mode.h>

class ModeModifyStoragePool : public Mode
{
   public:
      ModeModifyStoragePool():
         Mode() { }

      virtual int execute();

      static void printHelp();
};


#endif /* CTL_MODEMODIFYSTORAGEPOOL_H_ */
