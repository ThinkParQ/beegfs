//#include <program/Program.h>
//#include <common/net/message/session/AcquireAppendLockRespMsg.h>
//#include <session/SessionFileStore.h>
//#include "AcquireAppendLockMsgEx.h"
//
//
//bool AcquireAppendLockMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
//   char* respBuf, size_t bufLen, HighResolutionStats* stats)
//{
//   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername(); 
//   LOG_DEBUG("AcquireAppendLockMsg incoming", 4,
//      std::string("Received a AcquireAppendLockMsg from: ") + peer);
//   
////   const int acquireTimeoutMS = 10000;
////   AppendLockErr lockRes = AppendLockErr_INTERNAL;
////
////   SessionStore* sessions = Program::getApp()->getSessions();
////   Session* session = sessions->referenceSession(getSessionID(), true);
////   SessionFileStore* sessionFiles = session->getFiles();
////
////   SessionFile* sessionFile = sessionFiles->referenceSession(getFD() );
////   
////   try
////   {
////      if(!sessionFile)
////      { // sessionFile not open
////         LogContext log("AcquireAppendLockMsg incoming");
////         log.log(2, std::string("Unable to lock file (fd not open). ") +
////            std::string("SessionID: ") + getSessionID() );
////      }
////      else
////      { // sessionFile open => try to acquire the append lock
////         
////         bool acquired = sessionFile->acquireAppendLockNoWait();
////         while(!acquired)
////         { // lock is already acquired by another process => wait (and notify the client)
////            
////            AcquireAppendLockRespMsg respMsg(AppendLockErr_WAIT);
////            respMsg.serialize(respBuf, bufLen);
////            sock->send(respBuf, respMsg.getMsgLength(), 0);
////            
////            acquired = sessionFile->acquireAppendLockWait(acquireTimeoutMS);
////         }
////         
////         // the lock must be acquired at this point because of the while(!acquired)-loop above
////         
////         lockRes = AppendLockErr_SUCCESS;
////   
////         sessionFiles->releaseSession(sessionFile);
////         sessionFile = NULL;
////         
////      }
////      
////      sessions->releaseSession(session);
////      session = NULL;
////
////      AcquireAppendLockRespMsg respMsg(lockRes);
////      respMsg.serialize(respBuf, bufLen);
////      sock->send(respBuf, respMsg.getMsgLength(), 0);
////
////      return true;      
////   }
////   catch(SocketException& e)
////   {
////      if(lockRes == AppendLockErr_SUCCESS)
////      { // locked but socket exception occurred, so the client cannot know about it => unlock
////         sessionFile->releaseAppendLock();
////      }
////      
////      if(sessionFile)
////         sessionFiles->releaseSession(sessionFile);
////         
////      if(session)
////         sessions->releaseSession(session);
////
////      LogContext log("AcquireAppendLockMsg incoming");
////      log.log(2, std::string("Exception occurred.") +
////         std::string(" SessionID: ") + getSessionID() +
////         std::string(". Message: ") + e.what() );
////   }
//   
//   return false;
//
//}
//
//
