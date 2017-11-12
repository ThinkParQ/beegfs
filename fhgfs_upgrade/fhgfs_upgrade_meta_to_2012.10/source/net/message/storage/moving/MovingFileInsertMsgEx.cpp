#include <program/Program.h>
#include <common/net/message/storage/moving/MovingFileInsertMsg.h>
#include <common/net/message/storage/moving/MovingFileInsertRespMsg.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/MessagingTk.h>
#include "MovingFileInsertMsgEx.h"


bool MovingFileInsertMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   LogContext log("MovingFileInsertMsg incoming");

   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername(); 
   LOG_DEBUG_CONTEXT(log, Log_DEBUG, std::string("Received a MovingFileInsertMsg from: ") + peer);

   FileInode* unlinkedInode = NULL;

   FhgfsOpsErr insertRes = this->insert(&unlinkedInode); // create the new file here

   // prepare response data
   std::string unlinkedFileID;
   StripePattern* pattern;
   unsigned chunkHash;

   if(unlinkedInode)
   {
      unlinkedFileID = unlinkedInode->getEntryID();
      pattern        = unlinkedInode->getStripePattern();
      chunkHash      = unlinkedInode->getChunkHash();
   }
   else
   { // no unlinked file => use empty string as ID and create some fake data
      unlinkedFileID = "";
      UInt16Vector stripeTargetIDs;
      pattern = new Raid0Pattern(1, stripeTargetIDs, 0);
      chunkHash = 0;
   }
   
   MovingFileInsertRespMsg respMsg(insertRes, unlinkedFileID.c_str(), pattern, chunkHash);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );
   
   // cleanup
   if(unlinkedInode)
      delete(unlinkedInode);
   else
      delete(pattern);
      
   return true;
}


FhgfsOpsErr MovingFileInsertMsgEx::insert(FileInode** outUnlinkedFile)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   EntryInfo* fromFileInfo = this->getFromFileInfo();
   EntryInfo* toDirInfo    = this->getToDirInfo();
   std::string newName     = this->getNewName();

   FhgfsOpsErr moveRes = metaStore->moveRemoteFileInsert(
      fromFileInfo, toDirInfo->getEntryID(), newName, getSerialBuf(), outUnlinkedFile);
   
   return moveRes;
}
