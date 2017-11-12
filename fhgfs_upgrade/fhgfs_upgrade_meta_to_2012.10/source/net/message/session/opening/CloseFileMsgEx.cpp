#include <common/net/message/session/opening/CloseFileRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/SessionTk.h>
#include <program/Program.h>
#include <storage/MetaStore.h>
#include <net/msghelpers/MsgHelperClose.h>
#include "CloseFileMsgEx.h"


bool CloseFileMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   #ifdef FHGFS_DEBUG
      const char* logContext = "CloseFileMsg incoming";

      std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
      LOG_DEBUG(logContext, 4, std::string("Received a CloseFileMsg from: ") + peer);
   #endif // FHGFS_DEBUG

   EntryInfo* entryInfo = this->getEntryInfo();
   std::string closedFileID;
   bool unlinkDisposalFile = false;

   FhgfsOpsErr closeRes = MsgHelperClose::closeFile(this->getSessionID(), this->getFileHandleID(),
      entryInfo, this->getMaxUsedNodeIndex(), &closedFileID, &unlinkDisposalFile);

   // send early response
   CloseFileRespMsg respMsg(closeRes);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );


   // unlink if file marked as disposable
   if( (closeRes == FhgfsOpsErr_SUCCESS) && unlinkDisposalFile)
   { // check whether file has been unlinked (and perform the unlink operation on last close)

      // note: we do this only if also the local file close succeeded, because if the storage
      // servers are down, unlinkDisposableFile() will keep the file in the store anyways
      
      MsgHelperClose::unlinkDisposableFile(entryInfo->getEntryID() );
   }
   
   App* app = Program::getApp();
   app->getClientOpStats()->updateNodeOp(sock->getPeerIP(), MetaOpCounter_CLOSE);

   return true;
}

