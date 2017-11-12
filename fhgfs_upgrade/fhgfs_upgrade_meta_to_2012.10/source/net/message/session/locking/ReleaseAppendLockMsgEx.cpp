//#include <program/Program.h>
//#include <common/net/message/session/ReleaseAppendLockRespMsg.h>
//#include <session/SessionFileStore.h>
//#include "ReleaseAppendLockMsgEx.h"
//
//
//bool ReleaseAppendLockMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
//   char* respBuf, size_t bufLen, HighResolutionStats* stats)
//{
//   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername(); 
//   LOG_DEBUG("ReleaseAppendLockMsg incoming", 4,
//      std::string("Received a ReleaseAppendLockMsg from: ") + peer);
//      
//   bool releaseSuccess = false;
//   
//   SessionStore* sessions = Program::getApp()->getSessions();
//   Session* session = sessions->referenceSession(getSessionID(), true);
//   SessionFileStore* sessionFiles = session->getFiles();
//
//   SessionFile* sessionFile = sessionFiles->referenceSession(getFD() );
//   if(!sessionFile)
//   { // sessionFile not open
//      LogContext log("ReleaseAppendLockMsg incoming");
//      log.log(2, std::string("Unable to lock file (fd not open). ") +
//         std::string("SessionID: ") + getSessionID() );
//   }
//   else
//   { // sessionFile open => release the append lock
//      
//      sessionFile->releaseAppendLock();
//      releaseSuccess = true;
//
//      sessionFiles->releaseSession(sessionFile);
//   }
//
//   
//   sessions->releaseSession(session);
//   
//   ReleaseAppendLockRespMsg respMsg(!releaseSuccess);
//   respMsg.serialize(respBuf, bufLen);
//   sock->send(respBuf, respMsg.getMsgLength(), 0);
//
//   return true;      
//}
//
//
