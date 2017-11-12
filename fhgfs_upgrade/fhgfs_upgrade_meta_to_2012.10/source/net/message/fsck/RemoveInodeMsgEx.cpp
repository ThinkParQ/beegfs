#include <program/Program.h>
#include <common/net/message/fsck/RemoveInodeRespMsg.h>
#include "RemoveInodeMsgEx.h"


bool RemoveInodeMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats) throw(SocketException)
{
   #ifdef FHGFS_DEBUG
      const char* logContext = "RemoveInodeMsg incoming";

      std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
      LOG_DEBUG(logContext, 4, std::string("Received a RemoveInodeMsg from: ") + peer);
   #endif // FHGFS_DEBUG
   
   std::string entryID = getEntryID();
   DirEntryType entryType = getEntryType();

   MetaStore* metaStore = Program::getApp()->getMetaStore();

   FhgfsOpsErr rmRes = FhgfsOpsErr_INTERNAL;

   if (entryType == DirEntryType_DIRECTORY)
   {
      rmRes = metaStore->removeDirInode(entryID);
   }
   else
   {
      // FIXME Bernd / Christian: Needs entryInfo, do we have this info?
      uint16_t ownerNodeID = 0;
      std::string parentEntryID;
      std::string fileName;
      int flags = 0;

      // fake entry, but probably not sufficient
      EntryInfo entryInfo(ownerNodeID, parentEntryID, entryID, fileName, entryType, flags);

      rmRes = metaStore->unlinkInode(&entryInfo, NULL);
   }
   
   RemoveInodeRespMsg respMsg(rmRes);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   return true;      
}
