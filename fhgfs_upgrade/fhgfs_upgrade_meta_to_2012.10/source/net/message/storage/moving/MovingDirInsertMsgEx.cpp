#include <program/Program.h>
#include <common/net/message/storage/moving/MovingDirInsertMsg.h>
#include <common/net/message/storage/moving/MovingDirInsertRespMsg.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/MessagingTk.h>
#include "MovingDirInsertMsgEx.h"


bool MovingDirInsertMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   LogContext log("MovingDirInsertMsg incoming");

   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername(); 
   LOG_DEBUG_CONTEXT(log, 4, std::string("Received a MovingDirInsertMsg from: ") + peer);

   FhgfsOpsErr insertRes = insert();

   MovingDirInsertRespMsg respMsg(insertRes);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );
   
   return true;
}


FhgfsOpsErr MovingDirInsertMsgEx::insert()
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   
   FhgfsOpsErr retVal;
   
   EntryInfo* toDirInfo = this->getToDirInfo();

   // reference parent
   DirInode* parentDir = metaStore->referenceDir(toDirInfo->getEntryID(), true);
   if(!parentDir)
      return FhgfsOpsErr_PATHNOTEXISTS;

   /* create dir-entry and add information about its inode from the given buffer */

   std::string newName = this->getNewName();
   const char* buf = this->getSerialBuf();
   DirEntry* newDirEntry = new DirEntry(newName);
   if (!newDirEntry->deserializeDentry(buf) )
   {
      LogContext("File rename").logErr("Bug: Deserialization of remote buffer failed. Are all "
         "meta servers running with the same version?" );
      delete newDirEntry;

      metaStore->releaseDir(toDirInfo->getEntryID() );

      return FhgfsOpsErr_INTERNAL;
   }

   FhgfsOpsErr mkRes = parentDir->makeDirEntry(newDirEntry);
   
   switch(mkRes)
   {
      case FhgfsOpsErr_SUCCESS:
      case FhgfsOpsErr_EXISTS:
         retVal = mkRes;
         break;

      default:
         retVal = FhgfsOpsErr_INTERNAL;
         break;
   }
   
   // clean-up
   metaStore->releaseDir(toDirInfo->getEntryID() );

   return retVal;
}
