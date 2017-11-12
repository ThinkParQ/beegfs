#include <program/Program.h>
#include <common/net/message/storage/attribs/SetAttrRespMsg.h>
#include <common/net/message/storage/attribs/SetLocalAttrMsg.h>
#include <common/net/message/storage/attribs/SetLocalAttrRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <components/worker/SetLocalFileAttribsWork.h>
#include "SetAttrMsgEx.h"


bool SetAttrMsgEx::processIncoming(struct sockaddr_in* fromAddr, Socket* sock,
   char* respBuf, size_t bufLen, HighResolutionStats* stats)
{
   #ifdef FHGFS_DEBUG
      const char* logContext = "SetAttrMsgEx incoming";

      std::string peer = fromAddr ? Socket::ipaddrToStr(&fromAddr->sin_addr) : sock->getPeername();
      LOG_DEBUG(logContext, Log_DEBUG, std::string("Received a SetAttrMsg from: ") + peer);
   #endif // FHGFS_DEBUG

   App* app = Program::getApp();
   FhgfsOpsErr setAttrRes;
   
   EntryInfo* entryInfo = this->getEntryInfo();

   if(entryInfo->getParentEntryID().empty() ) // FIXME Bernd: testme
   { // special case: setAttr for root directory
      setAttrRes = setAttrRoot();
   }
   else
   {
      setAttrRes = setAttr(entryInfo);
   }
   
   
   SetAttrRespMsg respMsg(setAttrRes);
   respMsg.serialize(respBuf, bufLen);
   sock->sendto(respBuf, respMsg.getMsgLength(), 0,
      (struct sockaddr*)fromAddr, sizeof(struct sockaddr_in) );


   // update operation counters
   app->getClientOpStats()->updateNodeOp(sock->getPeerIP(), MetaOpCounter_SETATTR);

   return true;      
}


/**
 * Set new file/dir attributes
 */
FhgfsOpsErr SetAttrMsgEx::setAttr(EntryInfo* entryInfo)
{
   FhgfsOpsErr retVal;
   
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   if (DirEntryType_ISFILE(entryInfo->getEntryType() ) )
   {
      // we need to reference the inode first, as we want to use it several times
      FileInode* inode = metaStore->referenceFile(entryInfo);
      if (inode)
      {
         retVal = metaStore->setAttr(entryInfo, getValidAttribs(), getAttribs() );

         // note: we referenced the inode before the metadata update, so it must still be the
         // same and it it's safe to set chunk attribs
         if((retVal == FhgfsOpsErr_SUCCESS) &&
            (getValidAttribs() & (SETATTR_CHANGE_MODIFICATIONTIME | SETATTR_CHANGE_LASTACCESSTIME) ) )
         { // we have set inode attribs => send info to stripe nodes
            retVal = setChunkFileAttribs(inode);
         }

         metaStore->releaseFile(entryInfo->getParentEntryID(), inode);
      }
      else
         retVal = FhgfsOpsErr_PATHNOTEXISTS;
   }
   else
   { // so a directory
      retVal = metaStore->setAttr(entryInfo, getValidAttribs(), getAttribs() );
   }
   
   return retVal;
}

FhgfsOpsErr SetAttrMsgEx::setAttrRoot()
{
   DirInode* rootDir = Program::getApp()->getRootDir();

   uint16_t localNodeID = Program::getApp()->getLocalNode()->getNumID();

   if(localNodeID != rootDir->getOwnerNodeID() )
   {
      return FhgfsOpsErr_NOTOWNER;
   }

   if(!rootDir->setAttrData(getValidAttribs(), getAttribs() ) )
      return FhgfsOpsErr_INTERNAL;

   
   return FhgfsOpsErr_SUCCESS;
}

FhgfsOpsErr SetAttrMsgEx::setChunkFileAttribs(FileInode* file)
{
   StripePattern* pattern = file->getStripePattern();

   if( (pattern->getStripeTargetIDs()->size() > 1) || pattern->getMirrorTargetIDs() )
      return setChunkFileAttribsParallel(file);
   else
      return setChunkFileAttribsSequential(file);
}

/**
 * Note: This method does not work for mirrored files; use setChunkFileAttribsParallel() for those.
 */
FhgfsOpsErr SetAttrMsgEx::setChunkFileAttribsSequential(FileInode* file)
{
   const char* logContext = "Set chunk file attribs S";
   
   StripePattern* pattern = file->getStripePattern();
   const UInt16Vector* targetIDs = pattern->getStripeTargetIDs();
   TargetMapper* targetMapper = Program::getApp()->getTargetMapper();
   NodeStore* nodes = Program::getApp()->getStorageNodes();
   
   FhgfsOpsErr retVal = FhgfsOpsErr_SUCCESS;
   
   std::string fileID(file->getEntryID() );

   // send request to each node and receive the response message
   unsigned currentTargetIndex = 0;
   for(UInt16VectorConstIter iter = targetIDs->begin();
      iter != targetIDs->end();
      iter++, currentTargetIndex++)
   {
      uint16_t targetID = *iter;
      bool enableFileCreation = (currentTargetIndex == 0); // enable file creation of first node

      SetLocalAttrMsg setAttrMsg(fileID, targetID, getValidAttribs(), getAttribs(),
         enableFileCreation);

      Node* node = nodes->referenceNodeByTargetID(targetID, targetMapper);
      if(!node)
      {
         LogContext(logContext).log(Log_WARNING,
            "Unable to resolve storage node targetID: " + StringTk::uintToStr(targetID) + "; "
            "fileID: " + file->getEntryID() );

         if(retVal == FhgfsOpsErr_SUCCESS)
            retVal = FhgfsOpsErr_INTERNAL;
         
         continue;
      }
      
      // send request to node and receive response
      char* respBuf;
      NetMessage* respMsg;
      bool requestRes = MessagingTk::requestResponse(
         node, &setAttrMsg, NETMSGTYPE_SetLocalAttrResp, &respBuf, &respMsg);

      if(!requestRes)
      { // communication error
         LogContext(logContext).log(Log_WARNING,
            "Communication with node failed: " + node->getNodeIDWithTypeStr() + "; "
            "fileID: " + file->getEntryID() );

         if(retVal == FhgfsOpsErr_SUCCESS)
            retVal = FhgfsOpsErr_COMMUNICATION;
         
         nodes->releaseNode(&node);
         continue;
      }
      
      // correct response type received
      SetLocalAttrRespMsg* setRespMsg = (SetLocalAttrRespMsg*)respMsg;
      
      if(setRespMsg->getValue() != FhgfsOpsErr_SUCCESS)
      { // error: local file attribs not set
         LogContext(logContext).log(Log_WARNING,
            "Node was unable to set attribs of chunk file: " + node->getNodeIDWithTypeStr() + "; "
            "fileID: " + file->getEntryID() );
         
         if(retVal == FhgfsOpsErr_SUCCESS)
            retVal = (FhgfsOpsErr)setRespMsg->getValue();

         nodes->releaseNode(&node);
         delete(respMsg);
         free(respBuf);

         continue;
      }
      
      // success: local file attribs set
      LOG_DEBUG(logContext, Log_DEBUG,
         "Node has set attribs of chunk file: " + node->getNodeIDWithTypeStr() + "; " +
         "fileID: " + file->getEntryID() );
      
      nodes->releaseNode(&node);
      delete(respMsg);
      free(respBuf);
   }

   if(retVal != FhgfsOpsErr_SUCCESS)
   {
      LogContext(logContext).log(Log_WARNING,
         "Problems occurred during setting of chunk file attribs. "
         "fileID: " + file->getEntryID() );
   }

   return retVal;
}

FhgfsOpsErr SetAttrMsgEx::setChunkFileAttribsParallel(FileInode* file)
{
   const char* logContext = "Set chunk file attribs";

   App* app = Program::getApp();
   MultiWorkQueue* slaveQ = app->getCommSlaveQueue();
   StripePattern* pattern = file->getStripePattern();
   const UInt16Vector* targetIDs = pattern->getStripeTargetIDs();
   const UInt16Vector* mirrorTargetIDs = pattern->getMirrorTargetIDs(); // NULL if not raid10

   size_t numTargetWorks = targetIDs->size();
   size_t numMirrorWorks = mirrorTargetIDs ? numTargetWorks : 0;
   size_t numWorksTotal = numTargetWorks + numMirrorWorks;

   FhgfsOpsErrVec nodeResults(numWorksTotal); // 1st half for targets, 2nd half for mirrors
   SynchronizedCounter counter;

   // generate work for storage targets...

   for(size_t i=0; i < numTargetWorks; i++)
   {
      bool enableFileCreation = (i == 0); // enable file creation on first target

      SetLocalFileAttribsWork* work = new SetLocalFileAttribsWork(
         file->getEntryID(), getValidAttribs(), getAttribs(), enableFileCreation,
         (*targetIDs)[i], &(nodeResults[i]), &counter);

      slaveQ->addDirectWork(work);
   }

   // generate work for mirror targets...

   for(size_t i=0; i < numMirrorWorks; i++)
   {
      bool enableFileCreation = (i == 0); // enable file creation on first target

      SetLocalFileAttribsWork* work = new SetLocalFileAttribsWork(
         file->getEntryID(), getValidAttribs(), getAttribs(), enableFileCreation,
         (*mirrorTargetIDs)[i], &(nodeResults[numTargetWorks + i]), &counter);

      work->setMirroredFromTargetID( (*targetIDs)[i] );

      slaveQ->addDirectWork(work);
   }

   // wait for work completion...

   counter.waitForCount(numWorksTotal);

   // check target results...

   bool totalSuccess = true;

   for(size_t i=0; i < numTargetWorks; i++)
   {
      if(unlikely(nodeResults[i] != FhgfsOpsErr_SUCCESS) )
      {
         LogContext(logContext).log(Log_WARNING,
            "Problems occurred during setting of chunk file attribs. "
            "fileID: " + file->getEntryID() );

         totalSuccess = false;
         goto error_exit;
      }
   }

   // check mirror results...

   for(size_t i=0; i < numMirrorWorks; i++)
   {
      if(unlikely(nodeResults[numTargetWorks + i] != FhgfsOpsErr_SUCCESS) )
      {
         LogContext(logContext).log(Log_WARNING,
            "Problems occurred during setting of (mirror) chunk file attribs. "
            "fileID: " + file->getEntryID() );

         totalSuccess = false;
         goto error_exit;
      }
   }


error_exit:
   return totalSuccess ? FhgfsOpsErr_SUCCESS : FhgfsOpsErr_INTERNAL;
}
