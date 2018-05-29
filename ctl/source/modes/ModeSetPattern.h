#ifndef MODESETPATTERN_H_
#define MODESETPATTERN_H_

#include <common/Common.h>
#include "Mode.h"


class ModeSetPattern : public Mode
{
   public:
      ModeSetPattern() : actorUID(0)
      {}

      virtual int execute();

      static void printHelp();

   private:
      uint32_t actorUID;

      bool setPattern(Node& ownerNode, EntryInfo* entryInfo, unsigned chunkSize,
         unsigned numTargets, StoragePoolId storagePoolId, StripePatternType patternType);

};

#endif /*MODESETPATTERN_H_*/
