#ifndef CTL_MODEADDSTORAGEPOOL_H_
#define CTL_MODEADDSTORAGEPOOL_H_

#include <modes/Mode.h>

class ModeAddStoragePool : public Mode
{
   public:
      ModeAddStoragePool():
         Mode() { }

      virtual int execute();

      static void printHelp();
};


#endif /* CTL_MODEADDSTORAGEPOOL_H_ */
