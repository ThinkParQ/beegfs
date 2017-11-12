#include <program/Program.h>
#include <common/net/message/storage/creating/RmLocalDirRespMsg.h>
#include "RmLocalDirMsgEx.h"


bool RmLocalDirMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   #ifdef FHGFS_DEBUG
      const char* logContext = "RmLocalDirMsg incoming";

      std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
      LOG_DEBUG(logContext, 4, std::string("Received a RmLocalDirMsg from: ") + peer);
   #endif // FHGFS_DEBUG
   
   FhgfsOpsErr rmRes = rmDir();
   
   RmLocalDirRespMsg respMsg(rmRes);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   return true;      
}

FhgfsOpsErr RmLocalDirMsgEx::rmDir(void)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   
   EntryInfo* delEntryInfo = this->getDelEntryInfo();

   return metaStore->removeDirInode(delEntryInfo->getEntryID() );
}
