#ifndef MODELISTMIRRORBUDDYGROUPS_H_
#define MODELISTMIRRORBUDDYGROUPS_H_

#include <common/Common.h>
#include <common/nodes/TargetStateStore.h>
#include <modes/Mode.h>

class ModeListMirrorBuddyGroups : public Mode
{
   public:
      ModeListMirrorBuddyGroups()
      {
      }

      virtual int execute();

      static void printHelp();


   private:
      static void printGroups(const NodeType nodeType, const UInt16List& buddyGroupIDs, const UInt16List& primaryTargetIDs,
         const UInt16List& secondaryTargetIDs);
};

#endif /* MODELISTMIRRORBUDDYGROUPS_H_ */
