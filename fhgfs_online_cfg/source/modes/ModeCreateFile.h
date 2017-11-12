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

   private:

      static bool storagePoolHasTarget(const StoragePoolPtr& pool, uint16_t target)
      {
         return pool->hasTarget(target);
      }

      static bool storagePoolHasBuddyGroup(const StoragePoolPtr& pool, uint16_t target)
      {
         return pool->hasBuddyGroup(target);
      }
};

#endif /* MODECREATEFILE_H_ */
