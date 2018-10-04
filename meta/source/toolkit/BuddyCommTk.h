#ifndef BUDDYCOMMTK_H_
#define BUDDYCOMMTK_H_

#include <string>

#include <common/storage/StorageErrors.h>
#include <common/nodes/NumNodeID.h>

class MirrorBuddyGroupMapper;
class TimerQueue;

/**
 * This contains all the functions regarding the communication with the mirror buddy.
 * In the storage server, these mostly live in the StorageTargets class, but because the metadata
 * server only has a single metadata target at the moment, we group them here.
 */
namespace BuddyCommTk
{
   void prepareBuddyNeedsResyncState(Node& mgmtNode, const MirrorBuddyGroupMapper& bgm,
         TimerQueue& timerQ, NumNodeID localNodeID);

   void checkBuddyNeedsResync();
   void setBuddyNeedsResync(const std::string& path, bool needsResync);
   bool getBuddyNeedsResync();
};

#endif
