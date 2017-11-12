#include "UpdateFileAttribsMsgEx.h"

#include <net/msghelpers/MsgHelperStat.h>
#include <program/Program.h>

bool UpdateFileAttribsMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   const char* logContext = "UpdateFileAttribsMsg incoming";
   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG(logContext, 4, std::string("Received a UpdateFileAttribsMsg from: ") + peer);

   MetaStore* metaStore = Program::getApp()->getMetaStore();

   FsckFileInodeList inodes;
   FsckFileInodeList failedInodes;

   this->parseInodes(&inodes);

   for (FsckFileInodeListIter iter = inodes.begin(); iter != inodes.end(); iter++)
   {
      // create an EntryInfo object (NOTE: dummy fileName)
      EntryInfo entryInfo(iter->getSaveNodeID(), iter->getParentDirID(), iter->getID(), "",
         DirEntryType_REGULARFILE, 0);

      // set number of hardlinks
      FileInode* inode = metaStore->referenceFile(&entryInfo);
      if (inode)
      {
         inode->setNumHardlinksUnpersistent(iter->getNumHardLinks());
         inode->updateInodeOnDisk(&entryInfo);

         // call the dynamic attribs refresh method
         FhgfsOpsErr refreshRes = MsgHelperStat::refreshDynAttribs(&entryInfo, true);
         if (refreshRes != FhgfsOpsErr_SUCCESS)
         {
            LogContext(logContext).log(Log_WARNING, "Failed to update dynamic attributes of file. "
               "entryID: " + iter->getID());
            failedInodes.push_back(*iter);
         }

         /* only release it here, as refreshDynAttribs() also takes an inode reference and can
          * do the reference from in-memory data then */
         metaStore->releaseFile(entryInfo.getParentEntryID(), inode);
      }
   }

   UpdateFileAttribsRespMsg respMsg(&failedInodes);

   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0, (struct sockaddr*) fromAddr,
      sizeof(struct sockaddr_in));

   return true;
}
