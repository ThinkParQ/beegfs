#ifndef MODEREMOVETARGET_H_
#define MODEREMOVETARGET_H_

#include <common/storage/StorageErrors.h>
#include <common/Common.h>
#include "Mode.h"


class ModeRemoveTarget : public Mode
{
   public:
      ModeRemoveTarget()
      {
         cfgForce = false;
      }

      virtual int execute();

      static void printHelp();


   private:
      bool cfgForce;

      int removeTarget(uint16_t targetID);
      FhgfsOpsErr removeTargetComm(uint16_t targetID);
      FhgfsOpsErr isPartOfMirrorBuddyGroup(Node& mgmtNode, uint16_t targetID, bool* outIsPartOfMBG);

};


#endif /* MODEREMOVETARGET_H_ */
