#include <program/Program.h>
#include <common/net/message/storage/creating/RmDirEntryRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include "RmDirEntryMsgEx.h"


bool RmDirEntryMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   LogContext log("RmDirEntryMsg incoming");

   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
   LOG_DEBUG_CONTEXT(log, 4, std::string("Received a RmDirEntryMsg from: ") + peer);


   Path path;
   parsePath(&path);


   FhgfsOpsErr rmRes = rmDirEntry(path);

   RmDirEntryRespMsg respMsg(rmRes);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );

   App* app = Program::getApp();
   app->getClientOpStats()->updateNodeOp(sock->getPeerIP(), MetaOpCounter_RMLINK);

   return true;
}

FhgfsOpsErr RmDirEntryMsgEx::rmDirEntry(Path& path)
{
   const char* logContext = "RmDirEntryMsg (rm entry)";

   MetaStore* metaStore = Program::getApp()->getMetaStore();

   FhgfsOpsErr retVal;

   StringListConstIter pathIter = path.getPathElems()->begin();
   std::string parentID = *pathIter;
   std::string removeName = *(++pathIter);

   // reference parent
   DirInode* parentDir = metaStore->referenceDir(parentID, true);
   if(!parentDir)
      return FhgfsOpsErr_PATHNOTEXISTS;

   DirEntry removeDentry(removeName);
   bool getLinkRes = parentDir->getDentry(removeName, removeDentry);
   if(!getLinkRes)
      retVal = FhgfsOpsErr_PATHNOTEXISTS;
   else
   {
      if(DirEntryType_ISDIR(removeDentry.getEntryType() ) )
      { // remove dir link
         retVal = parentDir->removeDir(removeName, NULL);
      }
      else
      if(DirEntryType_ISFILE(removeDentry.getEntryType() ) )
      {
         retVal = parentDir->unlinkDirEntry(removeName, &removeDentry, true, NULL);
      }
      else
      { // unknown entry type
         LogContext(logContext).logErr(std::string("Unknown entry type: ") +
            StringTk::intToStr(removeDentry.getEntryType() ) );

         retVal = FhgfsOpsErr_INTERNAL;
      }
   }

   // clean-up
   metaStore->releaseDir(parentID);

   return retVal;
}

