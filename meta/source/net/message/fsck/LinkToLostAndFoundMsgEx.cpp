#include "LinkToLostAndFoundMsgEx.h"

#include <common/net/message/fsck/RemoveInodesMsg.h>
#include <common/net/message/fsck/RemoveInodesRespMsg.h>
#include <program/Program.h>
#include <toolkit/BuddyCommTk.h>

bool LinkToLostAndFoundMsgEx::processIncoming(ResponseContext& ctx)
{
   if (FsckDirEntryType_ISDIR(this->getEntryType()))
   {
      FsckDirEntryList createdDirEntries;
      FsckDirInodeList failedInodes;
      linkDirInodes(&failedInodes, &createdDirEntries);
      ctx.sendResponse(LinkToLostAndFoundRespMsg(&failedInodes, &createdDirEntries) );
   }
   else
   {
      LOG(COMMUNICATION, ERR, "LinkToLostAndFoundMsg received for non-inlined file inode.",
            ("from", ctx.peerName()));
      return false;
   }

   return true;
}

void LinkToLostAndFoundMsgEx::linkDirInodes(FsckDirInodeList* outFailedInodes,
   FsckDirEntryList* outCreatedDirEntries)
{
   const char* logContext = "LinkToLostAndFoundMsgEx (linkDirInodes)";

   NumNodeID localNodeNumID = Program::getApp()->getLocalNode().getNumID();

   FsckDirInodeList& dirInodes = getDirInodes();

   MetaStore* metaStore = Program::getApp()->getMetaStore();
   EntryLockStore* entryLockStore = Program::getApp()->getMirroredSessions()->getEntryLockStore();

   EntryInfo* lostAndFoundInfo = this->getLostAndFoundInfo();
   DirInode* lostAndFoundDir = metaStore->referenceDir(lostAndFoundInfo->getEntryID(),
      lostAndFoundInfo->getIsBuddyMirrored(), true);

   if ( !lostAndFoundDir )
   {
      *outFailedInodes = dirInodes;
      return;
   }
   else
   {
      for ( FsckDirInodeListIter iter = dirInodes.begin(); iter != dirInodes.end(); iter++ )
      {
         const std::string& entryID = iter->getID();
         NumNodeID ownerNodeID = iter->getOwnerNodeID();
         DirEntryType entryType = DirEntryType_DIRECTORY;
         DirEntry newDirEntry(entryType, entryID, entryID, ownerNodeID);

         FileIDLock lock;

         if (iter->getIsBuddyMirrored())
         {
            lock = {entryLockStore, entryID, true};
            newDirEntry.setBuddyMirrorFeatureFlag();
         }

         bool makeRes = lostAndFoundDir->makeDirEntry(newDirEntry);

         // stat the new file to get device and inode information
         std::string filename = MetaStorageTk::getMetaDirEntryPath(
               lostAndFoundInfo->getIsBuddyMirrored()
                  ? Program::getApp()->getBuddyMirrorDentriesPath()->str()
                  : Program::getApp()->getDentriesPath()->str(),
               lostAndFoundInfo->getEntryID()) + "/" + entryID;

         struct stat statBuf;

         int statRes = stat(filename.c_str(), &statBuf);

         int saveDevice;
         uint64_t saveInode;
         if ( likely(!statRes) )
         {
            saveDevice = statBuf.st_dev;
            saveInode = statBuf.st_ino;
         }
         else
         {
            saveDevice = 0;
            saveInode = 0;
            LogContext(logContext).log(Log_CRITICAL,
               "Could not stat dir entry file; entryID: " + entryID + ";filename: " + filename);
         }

         if ( makeRes != FhgfsOpsErr_SUCCESS )
            outFailedInodes->push_back(*iter);
         else
         {
            std::string parentID = lostAndFoundInfo->getEntryID();
            FsckDirEntry newFsckDirEntry(entryID, entryID, parentID, localNodeNumID,
               ownerNodeID, FsckDirEntryType_DIRECTORY, false, localNodeNumID,
               saveDevice, saveInode, lostAndFoundInfo->getIsBuddyMirrored());
            outCreatedDirEntries->push_back(newFsckDirEntry);
         }
      }

      lostAndFoundDir->refreshMetaInfo();
      metaStore->releaseDir(lostAndFoundInfo->getEntryID() );
   }
}
