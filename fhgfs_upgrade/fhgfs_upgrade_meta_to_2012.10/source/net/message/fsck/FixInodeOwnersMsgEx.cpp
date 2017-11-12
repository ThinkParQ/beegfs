#include "FixInodeOwnersMsgEx.h"

#include <common/fsck/FsckDirEntry.h>
#include <program/Program.h>

bool FixInodeOwnersMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   const char* logContext = "FixInodeOwnersMsgEx incoming";
   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG(logContext, 4, std::string("Received a FixInodeOwnersMsg from: ") + peer);

   MetaStore* metaStore = Program::getApp()->getMetaStore();

   FsckDirInodeList inodes;
   FsckDirInodeList failedInodes;

   this->parseInodes(&inodes);

   for ( FsckDirInodeListIter iter = inodes.begin(); iter != inodes.end(); iter++ )
   {
      std::string entryID = iter->getID();
      uint16_t ownerNodeID = iter->getOwnerNodeID();

      DirInode* dirInode = metaStore->referenceDir(entryID, true);

      if (unlikely(!dirInode))
      {
         LogContext(logContext).log(Log_WARNING, "Failed to update directory inode. Inode could"
            " not be referenced. entryID: " + entryID);
         continue; // continue to next entry
      }

      bool updateRes = dirInode->setOwnerNodeID(ownerNodeID);

      metaStore->releaseDir(entryID);

      if (!updateRes)
      {
         LogContext(logContext).log(Log_WARNING, "Failed to update directory inode. entryID: "
            + entryID);
         failedInodes.push_back(*iter);
      }
   }

   FixInodeOwnersRespMsg respMsg(&failedInodes);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   return true;
}
