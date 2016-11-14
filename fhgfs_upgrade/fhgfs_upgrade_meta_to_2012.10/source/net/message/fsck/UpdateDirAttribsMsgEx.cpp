#include "UpdateDirAttribsMsgEx.h"

#include <storage/MetaStore.h>
#include <program/Program.h>

bool UpdateDirAttribsMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   const char* logContext = "UpdateDirAttribsMsg incoming";
   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG(logContext, 4, std::string("Received a UpdateDirAttribsMsg from: ") + peer);

   MetaStore* metaStore = Program::getApp()->getMetaStore();

   FsckDirInodeList inodes;
   FsckDirInodeList failedInodes;

   this->parseInodes(&inodes);

   for (FsckDirInodeListIter iter = inodes.begin(); iter != inodes.end(); iter++)
   {
      // call the updating method
      std::string dirID = iter->getID();
      DirInode* dirInode = metaStore->referenceDir(dirID, true);

      if (!dirInode)
      {
         LogContext(logContext).logErr("Unable to reference directory; ID: " + iter->getID());
         failedInodes.push_back(*iter);
         continue;
      }

      FhgfsOpsErr refreshRes = dirInode->refreshMetaInfo();

      metaStore->releaseDir(dirID);

      if (refreshRes != FhgfsOpsErr_SUCCESS)
      {
         LogContext(logContext).log(Log_WARNING, "Failed to update attributes of directory. "
            "entryID: " + iter->getID());
         failedInodes.push_back(*iter);
      }

   }

   UpdateDirAttribsRespMsg respMsg(&failedInodes);

   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0, (struct sockaddr*) fromAddr,
      sizeof(struct sockaddr_in));

   return true;
}
