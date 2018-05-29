#ifndef CTL_MODEREMOVESTORAGEPOOL_H_
#define CTL_MODEREMOVESTORAGEPOOL_H_

#include <modes/Mode.h>

class ModeRemoveStoragePool : public Mode
{
   public:
      ModeRemoveStoragePool():
         Mode() { }

      virtual int execute();

      static void printHelp();
};


#endif /* CTL_MODEREMOVESTORAGEPOOL_H_ */
