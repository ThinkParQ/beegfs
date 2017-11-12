#include "ChangeStripeTargetMsgEx.h"
#include <program/Program.h>

bool ChangeStripeTargetMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG("ChangeStripeTargetMsg incoming", 4,
      std::string("Received a ChangeStripeTargetMsg from: ") + peer);

   LogContext log("ChangeStripeTargetMsg incoming");

   App* app = Program::getApp();
   MetaStore* metaStore = app->getMetaStore();

   FsckFileInodeList inodes;
   FsckFileInodeList failedInodes;

   this->parseInodes(&inodes);

   for ( FsckFileInodeListIter iter = inodes.begin(); iter != inodes.end(); iter++ )
   {
      // try to open the file
      EntryInfo entryInfo(app->getLocalNodeNumID(), iter->getParentDirID(), iter->getID(),
         "NONAME", DirEntryType_REGULARFILE, 0);

      FileInode* fileInode = metaStore->referenceFile(&entryInfo);

      if ( !fileInode )
      {
         log.logErr("Could not open file inode; entryID: " + iter->getID());
         failedInodes.push_back(*iter);
         continue; // try next inode
      }

      StripePattern* pattern = fileInode->getStripePattern();

      if ( !pattern )
      {
         log.logErr("Could not get stripe pattern for file inode; entryID: " + iter->getID());
         failedInodes.push_back(*iter);
         continue; // try next inode
      }

      this->changePattern(pattern);

      fileInode->updateInodeOnDisk(&entryInfo, pattern);

      metaStore->releaseFile(entryInfo.getParentEntryID(), fileInode);
   }

   ChangeStripeTargetRespMsg respMsg(&failedInodes);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0, (struct sockaddr*) fromAddr,
      sizeof(struct sockaddr_in));

   return true;
}

void ChangeStripeTargetMsgEx::changePattern(StripePattern* pattern)
{
   UInt16Vector* targetIDs = pattern->getStripeTargetIDsModifyable();

   for ( unsigned i = 0; i < targetIDs->size(); i++ )
   {
      if ( targetIDs->at(i) == this->getTargetID() )
      {
         targetIDs->at(i) = this->getNewTargetID();
      }
   }
}
