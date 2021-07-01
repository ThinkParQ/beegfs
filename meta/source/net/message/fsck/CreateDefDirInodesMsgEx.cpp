#include "CreateDefDirInodesMsgEx.h"
#include <program/Program.h>
#include <toolkit/BuddyCommTk.h>

bool CreateDefDirInodesMsgEx::processIncoming(ResponseContext& ctx)
{
   LogContext log("CreateDefDirInodesMsg incoming");

   App* app = Program::getApp();
   Config* cfg = app->getConfig();

   StringList failedInodeIDs;
   FsckDirInodeList createdInodes;

   for (auto it = items.begin(); it != items.end(); ++it)
   {
      const std::string& inodeID = std::get<0>(*it);
      const bool isBuddyMirrored = std::get<1>(*it);
      int mode = S_IFDIR | S_IRWXU;
      unsigned userID = 0; // root
      unsigned groupID = 0; // root
      const NumNodeID ownerNodeID = isBuddyMirrored
         ? NumNodeID(app->getMetaBuddyGroupMapper()->getLocalGroupID())
         : app->getLocalNode().getNumID();

      UInt16Vector stripeTargets;
      unsigned defaultChunkSize = cfg->getTuneDefaultChunkSize();
      unsigned defaultNumStripeTargets = cfg->getTuneDefaultNumStripeTargets();
      Raid0Pattern stripePattern(defaultChunkSize, stripeTargets, defaultNumStripeTargets);

      // we try to create a new directory inode, with default values

      FileIDLock dirLock;

      if (isBuddyMirrored)
         dirLock = {Program::getApp()->getMirroredSessions()->getEntryLockStore(), inodeID, true};

      DirInode dirInode(inodeID, mode, userID, groupID, ownerNodeID, stripePattern,
            isBuddyMirrored);
      if ( dirInode.storeAsReplacementFile(inodeID) == FhgfsOpsErr_SUCCESS )
      {
         // try to refresh the metainfo (maybe a .cont directory was already present)
         dirInode.refreshMetaInfo();

         StatData statData;
         dirInode.getStatData(statData);

         FsckDirInode fsckDirInode(inodeID, "", NumNodeID(), ownerNodeID, statData.getFileSize(),
            statData.getNumHardlinks(), stripeTargets, FsckStripePatternType_RAID0,
            ownerNodeID, isBuddyMirrored, true, false);
         createdInodes.push_back(fsckDirInode);
      }
      else
         failedInodeIDs.push_back(inodeID);
   }

   ctx.sendResponse(CreateDefDirInodesRespMsg(&failedInodeIDs, &createdInodes) );

   return true;
}
