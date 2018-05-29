#ifndef BUDDYCOMMTK_H_
#define BUDDYCOMMTK_H_

#include <string>

#include <common/storage/StorageErrors.h>

/**
 * This contains all the functions regarding the communication with the mirror buddy.
 * In the storage server, these mostly live in the StorageTargets class, but because the metadata
 * server only has a single metadata target at the moment, we group them here.
 */
namespace BuddyCommTk
{
   void checkBuddyNeedsResync();
   void setBuddyNeedsResync(const std::string& path, bool needsResync, NumNodeID buddyNodeID);
   void setBuddyNeedsResyncState(bool needsResync);
   bool getBuddyNeedsResync();

   void createBuddyNeedsResyncFile(const std::string& path, bool& outFileCreated,
         NumNodeID buddyNodeID);
   void removeBuddyNeedsResyncFile(const std::string& path);
};

#endif
