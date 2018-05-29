#ifndef MODEMAPTARGET_H_
#define MODEMAPTARGET_H_

#include <common/storage/StorageErrors.h>
#include <common/Common.h>
#include "Mode.h"


class ModeMapTarget : public Mode
{
   public:
      ModeMapTarget()
      {
      }

      virtual int execute();

      static void printHelp();


   private:
      int mapTarget(uint16_t targetID, uint16_t nodeID, StoragePoolId storagePoolId);
      FhgfsOpsErr mapTargetComm(uint16_t targetID, uint16_t nodeID, StoragePoolId storagePoolId);

};


#endif /* MODEMAPTARGET_H_ */
