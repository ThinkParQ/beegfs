#include "RetrieveDirEntriesMsgEx.h"

#include <common/storage/striping/Raid0Pattern.h>
#include <program/Program.h>

bool RetrieveDirEntriesMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   LogContext log("Incoming RetrieveDirEntriesMsg");
   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG_CONTEXT(log, 4, std::string("Received a RetrieveDirEntriesMsg from: ") + peer);

   unsigned hashDirNum = getHashDirNum();
   std::string currentContDirID = getCurrentContDirID();
   unsigned maxOutEntries = getMaxOutEntries();

   int64_t lastContDirOffset = getLastContDirOffset();
   int64_t lastHashDirOffset = getLastHashDirOffset();
   int64_t newHashDirOffset;
   int64_t newContDirOffset;

   FsckContDirList contDirsOutgoing;
   FsckDirEntryList dirEntriesOutgoing;
   FsckFileInodeList inlinedFileInodesOutgoing;

   unsigned readOutEntries = 0;

   uint16_t localNodeNumID = Program::getApp()->getLocalNode()->getNumID();
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   bool hasNext;

   if ( currentContDirID.empty() )
   {
      hasNext = StorageTkEx::getNextContDirID(hashDirNum, lastHashDirOffset, &currentContDirID,
         &newHashDirOffset);
      if ( hasNext )
      {
         lastHashDirOffset = newHashDirOffset;
         // we found a new .cont directory => send it to fsck
         FsckContDir contDir(currentContDirID, localNodeNumID);
         contDirsOutgoing.push_back(contDir);
      }
   }
   else
      hasNext = true;

   while ( hasNext )
   {
      std::string parentID = currentContDirID;

      unsigned remainingOutNames = maxOutEntries - readOutEntries;
      StringList entryNames;

      bool parentDirInodeIsTemp = false;
      DirInode* parentDirInode = metaStore->referenceDir(parentID, true);

      // it could be, that parentDirInode does not exist
      // in fsck we create a temporary inode for this case, so that we can modify the dentry
      // hopefully, the inode itself will get fixed later
      if ( unlikely(!parentDirInode) )
      {
         log.log(
            Log_NOTICE,
            "Could not reference directory. EntryID: " + parentID
               + " => using temporary directory inode ");

         // create temporary inode
         int mode = S_IFDIR | S_IRWXU;
         UInt16Vector stripeTargets;
         Raid0Pattern stripePattern(0, stripeTargets, 0);
         parentDirInode = new DirInode(parentID, mode, 0, 0,
            Program::getApp()->getLocalNode()->getNumID(), stripePattern);

         parentDirInodeIsTemp = true;
      }

      if ( parentDirInode->listIncremental(lastContDirOffset, 0, remainingOutNames, &entryNames,
         &newContDirOffset) == FhgfsOpsErr_SUCCESS )
      {
         lastContDirOffset = newContDirOffset;
      }
      else
      {
         log.log(Log_WARNING, "Could not list contents of directory. EntryID: " + parentID);
      }

      // actually process the entries
      for ( StringListIter namesIter = entryNames.begin(); namesIter != entryNames.end();
         namesIter++ )
      {
         std::string filename = MetaStorageTk::getMetaDirEntryPath(
            Program::getApp()->getStructurePath()->getPathAsStrConst(), parentID) + "/" +
               *namesIter;

         // create a EntryInfo and put the information into an FsckDirEntry object
         EntryInfo entryInfo;
         DentryInodeMeta inlinedInodeData;
         DirEntryType entryType = parentDirInode->getEntryInfo(*namesIter, &entryInfo,
            &inlinedInodeData);
         if ( entryType != DirEntryType_INVALID )
         {
            std::string dentryID = entryInfo.getEntryID();
            std::string dentryName = *namesIter;
            uint16_t dentryOwnerID = entryInfo.getOwnerNodeID();
            FsckDirEntryType fsckEntryType = FsckTk::DirEntryTypeToFsckDirEntryType(entryType);

            // stat the file to get device and inode information
            struct stat statBuf;

            int statRes = stat(filename.c_str(), &statBuf);

            int saveDevice;
            uint64_t saveInode;
            if (likely(!statRes))
            {
               saveDevice = statBuf.st_dev;
               saveInode = statBuf.st_ino;
            }
            else
            {
               saveDevice = 0;
               saveInode = 0;
               log.log(Log_CRITICAL, "Could not stat dir entry file; entryID: " + dentryID
                  + ";filename: " + filename);
            }

            FsckDirEntry fsckDirEntry(dentryID, dentryName, parentID, localNodeNumID, dentryOwnerID,
               fsckEntryType, localNodeNumID, saveDevice, saveInode);

            dirEntriesOutgoing.push_back(fsckDirEntry);
         }
         else
         {
            log.log(Log_WARNING, "Unable to create dir entry from entry with name " + *namesIter
                  + " in directory with ID " + parentID);
         }

         // now, if the inode data is inlined we create an fsck inode object here
         if ( (DirEntryType_ISFILE(entryType))
            && (entryInfo.getStatFlags() & ENTRYINFO_FLAG_INLINED) )
         {
            std::string inodeID = inlinedInodeData.getID();

            int mode;
            unsigned userID;
            unsigned groupID;

            int64_t fileSize;
            int64_t creationTime;
            int64_t modificationTime;
            int64_t lastAccessTime;
            unsigned numHardLinks;

            StatData* statData = inlinedInodeData.getInodeStatData();

            if ( statData )
            {
               mode = statData->getMode();
               userID = statData->getUserID();
               groupID = statData->getGroupID();
               fileSize = statData->getFileSize();
               creationTime = statData->getCreationTimeSecs();
               modificationTime = statData->getModificationTimeSecs();
               lastAccessTime = statData->getLastAccessTimeSecs();
               numHardLinks = statData->getNumHardlinks();
            }
            else
            {
               log.logErr(std::string("Unable to get stat data of inlined file inode: ") + inodeID
                     + ". SysErr: " + System::getErrString());
               mode = 0;
               userID = 0;
               groupID = 0;
               fileSize = 0;
               creationTime = 0;
               modificationTime = 0;
               lastAccessTime = 0;
               numHardLinks = 0;
            }

            UInt16Vector stripeTargets;
            unsigned chunkSize;
            FsckStripePatternType stripePatternType = FsckTk::stripePatternToFsckStripePattern(
               inlinedInodeData.getPattern(), &stripeTargets, &chunkSize);

            FsckFileInode fileInode(inodeID, parentID, localNodeNumID, mode, userID, groupID,
               fileSize, creationTime, modificationTime, lastAccessTime, numHardLinks,
               stripeTargets, stripePatternType, chunkSize, localNodeNumID);

            inlinedFileInodesOutgoing.push_back(fileInode);
         }
      }

      if ( parentDirInodeIsTemp )
         SAFE_DELETE(parentDirInode);
      else
         metaStore->releaseDir(parentID);

      if ( entryNames.size() < remainingOutNames )
      {
         // directory is at the end => proceed with next
         hasNext = StorageTkEx::getNextContDirID(hashDirNum, lastHashDirOffset, &currentContDirID,
            &newHashDirOffset);

         if ( hasNext )
         {
            lastHashDirOffset = newHashDirOffset;
            lastContDirOffset = 0;

            readOutEntries += entryNames.size();

            // we found a new .cont directory => send it to fsck
            FsckContDir contDir(currentContDirID, localNodeNumID);
            contDirsOutgoing.push_back(contDir);
         }
      }
      else
      {
         // there are more to come, but we need to exit the loop now, because maxCount is reached
         hasNext = false;
      }
   }

   RetrieveDirEntriesRespMsg respMsg(&contDirsOutgoing, &dirEntriesOutgoing,
      &inlinedFileInodesOutgoing, currentContDirID, lastHashDirOffset, lastContDirOffset);

   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0, (struct sockaddr*) fromAddr,
      sizeof(struct sockaddr_in));

   return true;
}
