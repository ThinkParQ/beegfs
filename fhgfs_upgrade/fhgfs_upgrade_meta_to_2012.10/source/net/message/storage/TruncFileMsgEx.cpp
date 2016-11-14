#include <common/toolkit/MessagingTk.h>
#include <common/net/message/storage/TruncFileRespMsg.h>
#include <net/msghelpers/MsgHelperTrunc.h>
#include "TruncFileMsgEx.h"
#include <program/Program.h>

bool TruncFileMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   #ifdef FHGFS_DEBUG
      const char* logContext = "TruncFileMsgEx incoming";

      std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
      LOG_DEBUG(logContext, 4, std::string("Received a TruncFileMsg from: ") + peer);
   #endif // FHGFS_DEBUG

   int64_t fileSize     = this->getFilesize();
   EntryInfo* entryInfo = this->getEntryInfo();

   FhgfsOpsErr truncRes = MsgHelperTrunc::truncFile(entryInfo, fileSize);
   
   TruncFileRespMsg respMsg(truncRes);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   App* app = Program::getApp();
   app->getClientOpStats()->updateNodeOp(sock->getPeerIP(), MetaOpCounter_TRUNCATE);

   return true;      
}

