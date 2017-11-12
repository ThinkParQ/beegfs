#include <common/net/message/storage/attribs/SetDirPatternRespMsg.h>
#include <common/storage/striping/Raid0Pattern.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include <storage/MetaStore.h>
#include "SetDirPatternMsgEx.h"


bool SetDirPatternMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   LogContext log("SetDirPatternMsgEx incoming");

   std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername(); 
   LOG_DEBUG_CONTEXT(log, 4, std::string("Received a SetDirPatternMsg from: ") + peer);

   EntryInfo* entryInfo = this->getEntryInfo();

   LOG_DEBUG("SetDirPatternMsgEx::processIncoming", 5, std::string("parentEntryID: ") +
      entryInfo->getParentEntryID() + " entryID: " + entryInfo->getEntryID() );
   
   StripePattern* pattern = createPattern();
   FhgfsOpsErr setPatternRes;
   
   if(pattern->getPatternType() == STRIPEPATTERN_Invalid)
   { // pattern is invalid
      setPatternRes = FhgfsOpsErr_INTERNAL;
      log.logErr("Received an invalid pattern");
   }
   else
   if( (pattern->getChunkSize() < STRIPEPATTERN_MIN_CHUNKSIZE) ||
       !MathTk::isPowerOfTwo(pattern->getChunkSize() ) )
   { // check of stripe pattern details validity failed

      setPatternRes = FhgfsOpsErr_INTERNAL;
      log.logErr("Received an invalid pattern chunksize: " +
         StringTk::uintToStr(pattern->getChunkSize() ) );
   }
   else
   { // received pattern is valid => apply it
      
      if(entryInfo->getEntryID().length() == 0 ) // FIXME Bernd: Testme
      { // special case: set pattern for root directory
         setPatternRes = setPatternRoot(pattern);
      }
      else
      {
         setPatternRes = setPatternRec(entryInfo, pattern);
      }
   }

   
   SetDirPatternRespMsg respMsg(setPatternRes);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );
   
   
   // cleanup
   delete(pattern);

   App* app = Program::getApp();
   app->getClientOpStats()->updateNodeOp(sock->getPeerIP(), MetaOpCounter_SETDIRPATTERN);

   return true;      
}


/**
 * @param pattern will be cloned and must be deleted by the caller 
 */
FhgfsOpsErr SetDirPatternMsgEx::setPatternRec(EntryInfo* entryInfo, StripePattern* pattern)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();
   
   FhgfsOpsErr retVal = FhgfsOpsErr_NOTADIR;
   
   DirInode* dir = metaStore->referenceDir(entryInfo->getEntryID(), true);
   if(dir)
   { // entry is a directory
      bool setPatternRes = dir->setStripePattern(*pattern);
      if(!setPatternRes)
      {
         LogContext log("SetDirPatternMsgEx::setPatternRec");
         log.logErr("Update of stripe pattern failed");
         
         retVal = FhgfsOpsErr_INTERNAL;
      }
      else
         retVal = FhgfsOpsErr_SUCCESS;

      metaStore->releaseDir(entryInfo->getEntryID() );
   }
   
   return retVal;
}

/**
 * @param pattern will be cloned and must be deleted by the caller 
 */
FhgfsOpsErr SetDirPatternMsgEx::setPatternRoot(StripePattern* pattern)
{
   DirInode* rootDir = Program::getApp()->getRootDir();

   uint16_t localNodeID = Program::getApp()->getLocalNode()->getNumID();

   if(localNodeID != rootDir->getOwnerNodeID() )
   {
      return FhgfsOpsErr_NOTOWNER;
   }
   
   bool setPatternRes = rootDir->setStripePattern(*pattern);
   if(!setPatternRes)
   {
      LogContext log("SetDirPatternMsgEx::setPatternRoot");
      log.logErr("Update of stripe pattern failed");
      
      return FhgfsOpsErr_INTERNAL;
   }
   
   return FhgfsOpsErr_SUCCESS;
}
