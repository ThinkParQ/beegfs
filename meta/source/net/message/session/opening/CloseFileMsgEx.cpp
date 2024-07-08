#include <common/components/streamlistenerv2/IncomingPreprocessedMsgWork.h>
#include <common/net/message/control/GenericResponseMsg.h>
#include <common/net/message/session/opening/CloseFileRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/SessionTk.h>
#include <components/FileEventLogger.h>
#include <net/msghelpers/MsgHelperClose.h>
#include <net/msghelpers/MsgHelperLocking.h>
#include <program/Program.h>
#include <session/EntryLock.h>
#include <storage/MetaStore.h>
#include "CloseFileMsgEx.h"

FileIDLock CloseFileMsgEx::lock(EntryLockStore& store)
{
   return {&store, getEntryInfo()->getEntryID(), true};
}

bool CloseFileMsgEx::processIncoming(ResponseContext& ctx)
{
#ifdef BEEGFS_DEBUG
   const char* logContext = "CloseFileMsg incoming";
#endif // BEEGFS_DEBUG

   LOG_DEBUG(logContext, Log_DEBUG,
      "BuddyMirrored: " + std::string(getEntryInfo()->getIsBuddyMirrored() ? "Yes" : "No") +
      " Secondary: " +
      std::string(hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond) ? "Yes" : "No") );

   // update operation counters (here on top because we have an early sock release in this msg)
   updateNodeOp(ctx, MetaOpCounter_CLOSE);

   return BaseType::processIncoming(ctx);
}

std::unique_ptr<MirroredMessageResponseState> CloseFileMsgEx::executeLocally(ResponseContext& ctx,
   bool isSecondary)
{
   if (isSecondary)
      return closeFileSecondary(ctx);
   else
      return closeFilePrimary(ctx);
}

std::unique_ptr<CloseFileMsgEx::ResponseState> CloseFileMsgEx::closeFilePrimary(
   ResponseContext& ctx)
{
   FhgfsOpsErr closeRes;
   bool unlinkDisposalFile = false;
   EntryInfo* entryInfo = getEntryInfo();
   bool modificationEventsMissed = true;
   bool closeSucceeded;

   if(isMsgHeaderFeatureFlagSet(CLOSEFILEMSG_FLAG_CANCELAPPENDLOCKS) )
   { // client requests cleanup of granted or pending locks for this handle

      unsigned ownerFD = SessionTk::ownerFDFromHandleID(getFileHandleID() );
      EntryLockDetails lockDetails(getClientNumID(), 0, 0, "", ENTRYLOCKTYPE_CANCEL);

      MsgHelperLocking::flockAppend(entryInfo, ownerFD, lockDetails);
   }

   /* two alternatives:
         1) early response before chunk file close (if client isn't interested in chunks result).
         2) normal response after chunk file close. */

   // if we are buddy mirrored, *do not* allow early close responses. since the primary will close
   // the chunk files (and update the inodes dynamic attributes), the secondary cannot do that.
   // thus we need to close all chunks, get the dynamic attributes, and push them to the secondary
   if (getEntryInfo()->getIsBuddyMirrored())
      unsetMsgHeaderFeatureFlag(CLOSEFILEMSG_FLAG_EARLYRESPONSE);

   if(isMsgHeaderFeatureFlagSet(CLOSEFILEMSG_FLAG_EARLYRESPONSE) )
   { // alternative 1: client requests an early response

      /* note: linux won't even return the vfs release() result to userspace apps, so there's
         usually no point in waiting for the chunk file close result before sending the response */

      unsigned accessFlags;
      MetaFileHandle inode;

      closeRes = MsgHelperClose::closeSessionFile(
         getClientNumID(), getFileHandleID(), entryInfo, &accessFlags, inode);
      closeSucceeded = closeRes == FhgfsOpsErr_SUCCESS;

      // send response
      earlyComplete(ctx, ResponseState(closeRes));

      modificationEventsMissed = inode->getNumHardlinks() > 1;

      // if session file close succeeds but chunk file close fails we should not attempt to
      // dispose. if chunk close failed for any other reason than network outages we might
      // put the storage server into an even weirder state by unlinking the chunk file:
      // the file itself is still open (has a session), but is gone on disk, and thus cannot
      // be reopened from stored sessions when the server is restarted.
      if(likely(closeRes == FhgfsOpsErr_SUCCESS) )
         closeRes = closeFileAfterEarlyResponse(std::move(inode), accessFlags, &unlinkDisposalFile);
   }
   else
   { // alternative 2: normal response (after chunk file close)

      closeRes = MsgHelperClose::closeFile(getClientNumID(), getFileHandleID(),
         entryInfo, getMaxUsedNodeIndex(), getMsgHeaderUserID(), &unlinkDisposalFile,
         &modificationEventsMissed, &dynAttribs, &inodeTimestamps);
      closeSucceeded = closeRes == FhgfsOpsErr_SUCCESS;

      if (getEntryInfo()->getIsBuddyMirrored() && getMaxUsedNodeIndex() >= 0)
         addMsgHeaderFeatureFlag(CLOSEFILEMSG_FLAG_DYNATTRIBS);

      //Avoid sending early response. Let unlink of Disposal file happens.
      //with Locks held. But make sure before file gets unlink we synchronise
      //the operation with any on-going buddy mirror resync operation.
      ResponseState responseState(closeRes);
      buddyResyncNotify(ctx, responseState.changesObservableState());
   }

   if (closeSucceeded && Program::getApp()->getFileEventLogger() && getFileEvent())
   {
         Program::getApp()->getFileEventLogger()->log(
                  *getFileEvent(),
                  entryInfo->getEntryID(),
                  entryInfo->getParentEntryID(),
                  "",
                  modificationEventsMissed);
   }

   // unlink if file marked as disposable
   if( (closeRes == FhgfsOpsErr_SUCCESS) && unlinkDisposalFile)
   { // check whether file has been unlinked (and perform the unlink operation on last close)

      /* note: we do this only if also the chunk file close succeeded, because if storage servers
         are down, unlinkDisposableFile() will keep the file in the disposal store anyways */

      // this only touches timestamps on the disposal dirinode, which is not visible to the user,
      // so no need to fix up timestamps that have diverged between primary and secondary
      MsgHelperClose::unlinkDisposableFile(entryInfo->getEntryID(), getMsgHeaderUserID(),
            entryInfo->getIsBuddyMirrored());
   }

   //for alternative 2: forward the operation to secondary
   //after unlinkDisposalFile on primary is complete.
   if(!isMsgHeaderFeatureFlagSet(CLOSEFILEMSG_FLAG_EARLYRESPONSE))
      return boost::make_unique<ResponseState>(closeRes);

   return {};
}

std::unique_ptr<CloseFileMsgEx::ResponseState> CloseFileMsgEx::closeFileSecondary(
   ResponseContext& ctx)
{
   if (isMsgHeaderFeatureFlagSet(CLOSEFILEMSG_FLAG_CANCELAPPENDLOCKS))
   {
      // client requests cleanup of granted or pending locks for this handle
      unsigned ownerFD = SessionTk::ownerFDFromHandleID(getFileHandleID() );
      EntryLockDetails lockDetails(getClientNumID(), 0, 0, "", ENTRYLOCKTYPE_CANCEL);

      MsgHelperLocking::flockAppend(getEntryInfo(), ownerFD, lockDetails);
   }

   // on secondary we only need to close the session and the meta file, because the chunk files
   // will be closed by primary

   unsigned accessFlags;
   MetaFileHandle inode;

   FhgfsOpsErr closeRes = MsgHelperClose::closeSessionFile(
      getClientNumID(), getFileHandleID(), getEntryInfo(), &accessFlags, inode);

   // the file may not be open on the secondary, in which case inode == nullptr
   if (closeRes != FhgfsOpsErr_SUCCESS)
      return boost::make_unique<ResponseState>(closeRes);

   // maybe assert?
   if (isMsgHeaderFeatureFlagSet(CLOSEFILEMSG_FLAG_DYNATTRIBS))
      inode->setDynAttribs(dynAttribs);

   fixInodeTimestamp(*inode, inodeTimestamps, getEntryInfo());

   unsigned numHardlinks;
   unsigned numInodeRefs;
   Program::getApp()->getMetaStore()->closeFile(getEntryInfo(), std::move(inode), accessFlags,
         &numHardlinks, &numInodeRefs);

   // unlink if file marked as disposable
   // this only touches timestamps on the disposal dirinode, which is not visible to the user,
   // so no need to fix up timestamps that have diverged between primary and secondary
   if( (closeRes == FhgfsOpsErr_SUCCESS) && (!numHardlinks) && (!numInodeRefs))
      MsgHelperClose::unlinkDisposableFile(getEntryInfo()->getEntryID(), getMsgHeaderUserID(),
            getEntryInfo()->getIsBuddyMirrored());

   return boost::make_unique<ResponseState>(closeRes);
}

/**
 * The rest after MsgHelperClose::closeSessionFile(), i.e. MsgHelperClose::closeChunkFile()
 * and metaStore->closeFile().
 *
 * @param inode inode for closed file (as returned by MsgHelperClose::closeSessionFile() )
 * @param maxUsedNodeIndex zero-based index, -1 means "none"
 * @param outFileWasUnlinked true if the hardlink count of the file was 0
 */
FhgfsOpsErr CloseFileMsgEx::closeFileAfterEarlyResponse(MetaFileHandle inode, unsigned accessFlags,
   bool* outUnlinkDisposalFile)
{
   MetaStore* metaStore = Program::getApp()->getMetaStore();

   unsigned numHardlinks;
   unsigned numInodeRefs;

   *outUnlinkDisposalFile = false;


   FhgfsOpsErr chunksRes = MsgHelperClose::closeChunkFile(
      getClientNumID(), getFileHandleID(), getMaxUsedNodeIndex(), *inode, getEntryInfo(),
      getMsgHeaderUserID(), NULL);


   metaStore->closeFile(getEntryInfo(), std::move(inode), accessFlags, &numHardlinks,
         &numInodeRefs);


   if (!numHardlinks && !numInodeRefs)
      *outUnlinkDisposalFile = true;

   return chunksRes;
}

void CloseFileMsgEx::forwardToSecondary(ResponseContext& ctx)
{
   sendToSecondary(ctx, *this, NETMSGTYPE_CloseFileResp);
}
