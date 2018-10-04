#ifndef MODEADDMIRRORBUDDYGROUP_H_
#define MODEADDMIRRORBUDDYGROUP_H_

#include <common/nodes/MirrorBuddyGroupCreator.h>
#include <common/Common.h>
#include <modes/Mode.h>
#include <program/Program.h>



class ModeAddMirrorBuddyGroup : public Mode, public MirrorBuddyGroupCreator
{
   public:
      ModeAddMirrorBuddyGroup() :
         MirrorBuddyGroupCreator(
            Program::getApp()->getMgmtNodes(),
            Program::getApp()->getMetaNodes(), &Program::getApp()->getMetaRoot(),
            Program::getApp()->getStoragePoolStore()),
         cfgAutomatic(false) { }

      virtual int execute();

      static void printHelp();


   private:
      bool cfgAutomatic;

      FhgfsOpsErr doAutomaticMode();
      void printMirrorBuddyGroups(UInt16List* buddyGroupIDs, UInt16List* primaryIDs,
         UInt16List* secondaryIDs);
      void printMirrorBuddyGroups(UInt16List* buddyGroupIDs, MirrorBuddyGroupList* buddyGroups);
      void printAutomaticResults(FhgfsOpsErr retValGeneration, UInt16List* newBuddyGroupIDs,
         MirrorBuddyGroupList* newBuddyGroups, UInt16List* oldBuddyGroupIDs,
         UInt16List* oldPrimaryIDs, UInt16List* oldSecondaryIDs);
};


#endif /* MODEADDMIRRORBUDDYGROUP_H_ */
