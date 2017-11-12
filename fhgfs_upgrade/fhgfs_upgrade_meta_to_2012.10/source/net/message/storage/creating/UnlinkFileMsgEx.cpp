#include <common/toolkit/MessagingTk.h>
#include <common/net/message/storage/creating/UnlinkFileRespMsg.h>
#include <common/net/message/storage/creating/UnlinkFileMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileRespMsg.h>
#include <net/msghelpers/MsgHelperUnlink.h>
#include <program/Program.h>
#include "UnlinkFileMsgEx.h"


bool UnlinkFileMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   #ifdef FHGFS_DEBUG
      const char* logContext = "UnlinkFileMsg incoming";

      std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
      LOG_DEBUG(logContext, 4, std::string("Received a UnlinkFileMsg from: ") + peer);
   #endif // FHGFS_DEBUG
   
   EntryInfo* parentInfo = this->getParentInfo();
   std::string removeName = this->getDelFileName();

   LOG_DEBUG(logContext, 4, "ParentID: " + parentInfo->getEntryID() +
      " deleteName: " + removeName  );

   FhgfsOpsErr rmRes = MsgHelperUnlink::unlinkFile(parentInfo->getEntryID(), removeName);
   
   UnlinkFileRespMsg respMsg(rmRes);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   App* app = Program::getApp();
   app->getClientOpStats()->updateNodeOp(sock->getPeerIP(), MetaOpCounter_UNLINK);

   return true;      
}
