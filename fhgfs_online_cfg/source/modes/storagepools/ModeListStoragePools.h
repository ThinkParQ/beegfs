#ifndef CTL_MODELISTSTORAGEPOOLS_H_
#define CTL_MODELISTSTORAGEPOOLS_H_

#include <modes/Mode.h>

class ModeListStoragePools : public Mode
{
   public:
      ModeListStoragePools():
         Mode() { }

      virtual int execute();

      static void printHelp();

   private:
      void printPools(const StoragePoolPtrVec& pools) const;
};


#endif /* CTL_MODELISTSTORAGEPOOLS_H_ */
