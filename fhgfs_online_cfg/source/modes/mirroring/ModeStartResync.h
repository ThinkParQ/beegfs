#ifndef MODESTARTSTORAGERESYNC_H_
#define MODESTARTSTORAGERESYNC_H_

#include <common/storage/StorageErrors.h>
#include <common/Common.h>
#include <modes/Mode.h>

class ModeStartResync : public Mode
{
   public:
      ModeStartResync()
      {
      }

      virtual int execute();

      static void printHelp();


   private:
      NodeType nodeType;

      int requestResync(uint16_t syncToTargetID, bool isBuddyGroupID, int64_t timestamp,
         bool restartResync);
      FhgfsOpsErr setResyncState(uint16_t targetID, NumNodeID nodeID);
      FhgfsOpsErr overrideLastBuddyComm(uint16_t targetID, NumNodeID nodeID, int64_t timestamp,
         bool restartResync);
};


#endif /* MODESTARTSTORAGERESYNC_H_ */
