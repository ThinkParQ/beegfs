#include <common/toolkit/MessagingTk.h>
#include <common/net/message/storage/creating/UnlinkLocalFileMsg.h>
#include <common/net/message/storage/creating/UnlinkLocalFileRespMsg.h>
#include <components/worker/UnlinkLocalFileWork.h>
#include <net/msghelpers/MsgHelperMkFile.h>
#include <program/Program.h>
#include "MsgHelperUnlink.h"


FhgfsOpsErr MsgHelperUnlink::unlinkFile(std::string parentID, std::string removeName)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   FhgfsOpsErr retVal;

   FileInode* unlinkedInode = NULL;

   retVal = metaStore->unlinkFile(parentID, removeName, &unlinkedInode);

   // if the file is still opened unlinkedInode will be NULL even on FhgfsOpsErr_SUCCESS
   if( (retVal == FhgfsOpsErr_SUCCESS) && unlinkedInode)
   {
      // FIXME Bernd: Write a unit test.
      return unlinkChunkFiles(unlinkedInode);
   }

   return retVal;
}

/**
 * Handle an unlinked inode. Will unlink (storage) chunk files.
 *
 * Note: Destroys (deletes) the given inode object.
 */
FhgfsOpsErr MsgHelperUnlink::unlinkChunkFiles(FileInode* unlinkedInode)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   FhgfsOpsErr retVal;

   retVal = unlinkChunkFilesInternal(unlinkedInode);
   if(retVal != FhgfsOpsErr_SUCCESS)
   {  /* Failed to unlink storage chunk files => add file to the disposable store to try
       * again later. */
      metaStore->insertDisposableFile(unlinkedInode); // destructs unlinkedInode
   }
   else
   { // success (local files unlinked)
      delete(unlinkedInode);
   }

   return retVal;
}


FhgfsOpsErr MsgHelperUnlink::unlinkChunkFilesInternal(FileInode* file)
{
   StripePattern* pattern = file->getStripePattern();

   if( (pattern->getStripeTargetIDs()->size() > 1) || pattern->getMirrorTargetIDs() )
      return unlinkLocalFileParallel(file);
   else
      return unlinkLocalFileSequential(file);
}

/**
 * Note: This method does not work for mirrored files; use unlinkLocalFileParallel() for those.
 */
FhgfsOpsErr MsgHelperUnlink::unlinkLocalFileSequential(FileInode* file)
{
   std::string logContext("Unlink Helper (unlink chunk file S [" + file->getEntryID() + "])");

   StripePattern* pattern = file->getStripePattern();
   const UInt16Vector* targetIDs = pattern->getStripeTargetIDs();
   TargetMapper* targetMapper = Program::getApp()->getTargetMapper();
   NodeStore* nodes = Program::getApp()->getStorageNodes();

   std::string fileID(file->getEntryID() );

   // send request to each node and receive the response message
   unsigned unlinkedOnAllNodes = true;
   for(UInt16VectorConstIter iter = targetIDs->begin();
      iter != targetIDs->end();
      iter++)
   {
      uint16_t targetID = *iter;

      Node* node = nodes->referenceNodeByTargetID(targetID, targetMapper);
      if(!node)
      {
         // note: not an error, because we don't care about removed nodes
         LogContext(logContext).log(Log_WARNING,
            "Unable to resolve storage node targetID: " + StringTk::uintToStr(targetID) );

         continue;
      }

      UnlinkLocalFileMsg unlinkMsg(fileID, targetID);

      // send request to node and receive response
      char* respBuf;
      NetMessage* respMsg;
      bool requestRes = MessagingTk::requestResponse(
         node, &unlinkMsg, NETMSGTYPE_UnlinkLocalFileResp, &respBuf, &respMsg);

      if(!requestRes)
      { // communication error
         LogContext(logContext).log(Log_WARNING,
            "Communication with storage node failed: " + node->getTypedNodeID() );

         nodes->releaseNode(&node);
         unlinkedOnAllNodes = false;
         continue;
      }

      // correct response type received
      UnlinkLocalFileRespMsg* unlinkRespMsg = (UnlinkLocalFileRespMsg*)respMsg;

      bool unlinkSuccess = (unlinkRespMsg->getValue() == 0);
      if(!unlinkSuccess)
      { // error: local file not unlinked
         LogContext(logContext).log(Log_WARNING,
            "Storage node was unable to unlink local file: " + node->getTypedNodeID() );

         nodes->releaseNode(&node);
         delete(respMsg);
         free(respBuf);
         unlinkedOnAllNodes = false;
         continue;
      }

      // success: local file unlinked
      LOG_DEBUG(logContext, Log_DEBUG,
         "Storage node unlinked local file: " + node->getTypedNodeID() );

      nodes->releaseNode(&node);
      delete(respMsg);
      free(respBuf);
   }

   if(!unlinkedOnAllNodes)
   {
      LogContext(logContext).log(Log_WARNING,
         std::string("Problems occurred during unlinking of the chunk files.") );

      return FhgfsOpsErr_INTERNAL;
   }

   return FhgfsOpsErr_SUCCESS;
}

FhgfsOpsErr MsgHelperUnlink::unlinkLocalFileParallel(FileInode* file)
{
   std::string logContext("Unlink Helper (unlink chunk file [" + file->getEntryID() + "])");

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
      UnlinkLocalFileWork* work = new UnlinkLocalFileWork(file->getEntryID(), (*targetIDs)[i],
         &(nodeResults[i]), &counter);

      slaveQ->addDirectWork(work);
   }

   // generate work for mirror targets...

   for(size_t i=0; i < numMirrorWorks; i++)
   {
      UnlinkLocalFileWork* work = new UnlinkLocalFileWork(file->getEntryID(), (*mirrorTargetIDs)[i],
         &(nodeResults[numTargetWorks + i]), &counter);

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
         if( (nodeResults[i] == FhgfsOpsErr_UNKNOWNNODE) ||
             (nodeResults[i] == FhgfsOpsErr_UNKNOWNTARGET) )
         { /* we don't return this as an error to the user, because the node/target was probably
              removed intentionally (and either way the rest of this file is lost now) */
            continue;
         }

         LogContext(logContext).log(Log_WARNING,
            "Problems occurred during unlinking of the chunk files.");

         totalSuccess = false;
         goto error_exit;
      }
   }

   // check mirror results...

   for(size_t i=0; i < numMirrorWorks; i++)
   {
      if(unlikely(nodeResults[numTargetWorks + i] != FhgfsOpsErr_SUCCESS) )
      {
         if( (nodeResults[numTargetWorks + i] == FhgfsOpsErr_UNKNOWNNODE) ||
             (nodeResults[numTargetWorks + i] == FhgfsOpsErr_UNKNOWNTARGET) )
         { /* we don't return this as an error to the user, because the node/target was probably
              removed intentionally (and either way the rest of this file is lost now) */
            continue;
         }

         LogContext(logContext).log(Log_WARNING,
            "Problems occurred during unlinking of storage server (mirror) chunk files.");

         totalSuccess = false;
         goto error_exit;
      }
   }


error_exit:
   return totalSuccess ? FhgfsOpsErr_SUCCESS : FhgfsOpsErr_INTERNAL;
}
