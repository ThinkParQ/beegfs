#include "FixInodeOwnersInDentryMsgEx.h"

#include <common/fsck/FsckDirEntry.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <program/Program.h>

bool FixInodeOwnersInDentryMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG("FixInodeOwnersInDentryMsg incoming", 4,
      std::string("Received a FixInodeOwnersInDentryMsg from: ") + peer);
   LogContext log("FixInodeOwnersInDentryMsgEx");

   MetaStore* metaStore = Program::getApp()->getMetaStore();

   FsckDirEntryList dentries;
   FsckDirEntryList failedEntries;

   this->parseDentries(&dentries);

   for ( FsckDirEntryListIter iter = dentries.begin(); iter != dentries.end(); iter++ )
   {
      std::string parentID = iter->getParentDirID();
      std::string entryName = iter->getName();
      uint16_t inodeOwner = iter->getInodeOwnerNodeID();

      bool parentDirInodeIsTemp = false;
      DirInode* parentDirInode = metaStore->referenceDir(parentID, true);

      // it could be, that parentDirInode does not exist
      // in fsck we create a temporary inode for this case, so that we can modify the dentry
      // hopefully, the inode itself will get fixed later
      if (unlikely(!parentDirInode))
      {
         log.log(Log_NOTICE,
            "Failed to update directory entry. Parent directory could not be "
               "referenced. parentID: " + parentID + " entryName: " + entryName
               + " => Using temporary inode");

         // create temporary inode
         int mode = S_IFDIR | S_IRWXU;
         UInt16Vector stripeTargets;
         Raid0Pattern stripePattern(0, stripeTargets, 0);
         parentDirInode = new DirInode(parentID, mode, 0, 0,
            Program::getApp()->getLocalNode()->getNumID(), stripePattern);

         parentDirInodeIsTemp = true;
      }

      FhgfsOpsErr updateRes = parentDirInode->setOwnerNodeID(entryName, inodeOwner);

      if (updateRes != FhgfsOpsErr_SUCCESS )
      {
         log.log(Log_WARNING, "Failed to update directory entry. parentID: " + parentID +
            " entryName: " + entryName);
         failedEntries.push_back(*iter);
      }

      if (parentDirInodeIsTemp)
         SAFE_DELETE(parentDirInode);
      else
         metaStore->releaseDir(parentID);
   }

   FixInodeOwnersInDentryRespMsg respMsg(&failedEntries);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   return true;
}
