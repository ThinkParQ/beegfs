#include "RecreateDentriesMsgEx.h"

#include <common/fsck/FsckDirEntry.h>
#include <common/fsck/FsckFsID.h>
#include <program/Program.h>

bool RecreateDentriesMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG("RecreateDentriesMsg incoming", 4,
      std::string("Received a RecreateDentriesMsg from: ") + peer);

   LogContext log("RecreateDentriesMsgEx");

   App* app = Program::getApp();
   uint16_t localNodeID = app->getLocalNodeNumID();
   MetaStore* metaStore = app->getMetaStore();

   FsckFsIDList fsIDs;
   FsckFsIDList failedCreates;
   FsckDirEntryList createdDentries;
   FsckFileInodeList createdInodes;

   this->parseFsIDs(&fsIDs);

   for ( FsckFsIDListIter iter = fsIDs.begin(); iter != fsIDs.end(); iter++ )
   {
      std::string parentPath = MetaStorageTk::getMetaDirEntryPath(
         app->getStructurePath()->getPathAsStrConst(), iter->getParentDirID());

      std::string dirEntryIDFilePath = MetaStorageTk::getMetaDirEntryIDPath(parentPath) + "/"
         + iter->getID();

      // the name is lost, so we take the ID as new name
      std::string dirEntryNameFilePath = parentPath + "/" + iter->getID();

      // before we link, let's see if we can open the parent dir, otherwise we should not mess
      // around here
      std::string dirID = iter->getParentDirID();
      DirInode* parentDirInode = metaStore->referenceDir(dirID, false);

      if (!parentDirInode)
      {
         log.logErr("Unable to reference parent directory; ID: " + iter->getParentDirID());
         failedCreates.push_back(*iter);
         continue;
      }

      // link the dentry-by-name file
      int linkRes = link(dirEntryIDFilePath.c_str(), dirEntryNameFilePath.c_str());

      if ( linkRes )
      {
         // error occured while linking
         log.logErr(
            "Failed to link dentry file; ParentID: " + iter->getParentDirID() + "; ID: "
               + iter->getID());
         failedCreates.push_back(*iter);
         continue;
      }

      // linking was OK => gather dentry (and inode) data, so fsck can add it

      DirEntry dirEntry(iter->getID());
      parentDirInode->getDentry(iter->getID(), dirEntry);

      // create the FsckDirEntry
      FsckDirEntry fsckDirEntry(dirEntry.getID(), dirEntry.getName(), iter->getParentDirID(),
         localNodeID, localNodeID, FsckTk::DirEntryTypeToFsckDirEntryType(dirEntry.getEntryType()),
         localNodeID, iter->getSaveDevice(), iter->getSaveInode());
      createdDentries.push_back(fsckDirEntry);

      // inlined inode?
      if ( dirEntry.isInodeInlined() )
      {
         DentryInodeMeta* inodeData = dirEntry.getInodeData();

         if ( inodeData )
         {
            UInt16Vector targetIDs;
            unsigned chunkSize;
            FsckStripePatternType fsckStripePatternType = FsckTk::stripePatternToFsckStripePattern(
               inodeData->getPattern(), &targetIDs, &chunkSize);

            FsckFileInode fsckFileInode(inodeData->getID(), iter->getParentDirID(),
               localNodeID, inodeData->getInodeStatData(), targetIDs, fsckStripePatternType,
               chunkSize, localNodeID);

            createdInodes.push_back(fsckFileInode);
         }
         else
         {
            log.logErr(
               "No inlined inode data; parentID: " + iter->getParentDirID() + "; ID: "
                  + iter->getID());
         }
      }

      metaStore->releaseDir(dirID);
   }

   RecreateDentriesRespMsg respMsg(&failedCreates, &createdDentries, &createdInodes);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0, (struct sockaddr*) fromAddr,
      sizeof(struct sockaddr_in));

   return true;
}
