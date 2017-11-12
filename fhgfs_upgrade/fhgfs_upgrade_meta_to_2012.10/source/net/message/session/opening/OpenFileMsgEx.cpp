#include <common/net/message/session/opening/OpenFileRespMsg.h>
#include <common/toolkit/SessionTk.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <net/msghelpers/MsgHelperOpen.h>
#include <program/Program.h>
#include "OpenFileMsgEx.h"


bool OpenFileMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   #ifdef FHGFS_DEBUG
      const char* logContext = "OpenFileMsg incoming";

      std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
      LOG_DEBUG(logContext, Log_DEBUG, std::string("Received a OpenFileMsg from: ") + peer);
   #endif // FHGFS_DEBUG

   App* app = Program::getApp();
   SessionStore* sessions = app->getSessions();

   EntryInfo* entryInfo = this->getEntryInfo();
   FileInode* inode;

   LOG_DEBUG(logContext, Log_SPAM, "ParentInfo: " + entryInfo->getParentEntryID() +
      " EntryID: " + entryInfo->getEntryID() + " FileName: " + entryInfo->getFileName() );

   FhgfsOpsErr openRes = MsgHelperOpen::openFile(entryInfo, getAccessFlags(), &inode);

   if(openRes != FhgfsOpsErr_SUCCESS)
   { // error occurred
      Raid0Pattern dummyPattern(1, UInt16Vector() );

      // send response
      OpenFileRespMsg respMsg(openRes, "", &dummyPattern);
      respMsg.serialize(respBuf, bufLen);
      sock->sendto(respBuf, respMsg.getMsgLength(), 0,
         (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

      app->getClientOpStats()->updateNodeOp(sock->getPeerIP(), MetaOpCounter_OPEN);

      return true;
   }

   // success => insert session
   SessionFile* sessionFile = new SessionFile(inode, getAccessFlags(), entryInfo);

   Session* session = sessions->referenceSession(getSessionID(), true);

   StripePattern* pattern = inode->getStripePattern();
   unsigned ownerFD = session->getFiles()->addSession(sessionFile);

   sessions->releaseSession(session);

   std::string fileHandleID = SessionTk::generateFileHandleID(ownerFD, entryInfo->getEntryID() );

   // send response
   OpenFileRespMsg respMsg(openRes, fileHandleID, pattern);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   app->getClientOpStats()->updateNodeOp(sock->getPeerIP(), MetaOpCounter_OPEN);

   return true;
}

