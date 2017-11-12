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
      void printGroups(NodeType nodeType, UInt16List& buddyGroupIDs, UInt16List& primaryTargetIDs,
         UInt16List& secondaryTargetIDs);
};

#endif /* MODELISTMIRRORBUDDYGROUPS_H_ */
