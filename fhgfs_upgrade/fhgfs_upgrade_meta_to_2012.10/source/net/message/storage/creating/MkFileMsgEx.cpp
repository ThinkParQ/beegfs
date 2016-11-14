#include <common/net/message/storage/creating/MkFileRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/storage/StatData.h>
#include <net/msghelpers/MsgHelperMkFile.h>
#include <program/Program.h>
#include "MkFileMsgEx.h"


bool MkFileMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   #ifdef FHGFS_DEBUG
      const char* logContext = "MkFileMsg incoming";

      std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
      LOG_DEBUG(logContext, Log_DEBUG, std::string("Received a MkFileMsg from: ") + peer);
   #endif // FHGFS_DEBUG

   LOG_DEBUG(logContext, Log_SPAM, "parentEntryID: " +
      this->getParentInfo()->getEntryID() + " newFileName: " + this->getNewName() );

   MkFileDetails mkDetails(this->getNewName(), getUserID(), getGroupID(), getMode() );

   UInt16List preferredNodes;
   parsePreferredNodes(&preferredNodes);

   EntryInfo entryInfo;
   DentryInodeMeta inodeData;

   FhgfsOpsErr mkRes = MsgHelperMkFile::mkFile(this->getParentInfo(), &mkDetails, &preferredNodes,
      &entryInfo, &inodeData);

   MkFileRespMsg respMsg(mkRes, &entryInfo );
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   App* app = Program::getApp();
   app->getClientOpStats()->updateNodeOp(sock->getPeerIP(), MetaOpCounter_MKFILE);

   return true;
}

