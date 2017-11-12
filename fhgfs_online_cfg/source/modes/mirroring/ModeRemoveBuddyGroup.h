#ifndef MODEREMOVEBUDDYGROUP_H
#define MODEREMOVEBUDDYGROUP_H

#include <common/Common.h>
#include <modes/Mode.h>
#include <program/Program.h>

class ModeRemoveBuddyGroup : public Mode
{
   public:
      virtual int execute();

      static void printHelp();

   private:
      FhgfsOpsErr removeGroup(const uint16_t groupID, const bool dryRun, const bool force);
};


#endif /* MODEADDMIRRORBUDDYGROUP_H_ */
