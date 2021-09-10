#ifndef META_NET_MESSAGE_MIRROREDMESSAGE_H
#define META_NET_MESSAGE_MIRROREDMESSAGE_H

#include <app/App.h>
#include <common/app/log/Logger.h>
#include <common/components/streamlistenerv2/IncomingPreprocessedMsgWork.h>
#include <common/net/message/session/AckNotifyMsg.h>
#include <common/net/message/session/AckNotifyRespMsg.h>
#include <common/net/message/NetMessage.h>
#include <common/storage/StorageErrors.h>
#include <common/toolkit/DebugVariable.h>
#include <common/toolkit/MessagingTk.h>
#include <program/Program.h>
#include <session/MirrorMessageResponseState.h>
#include <toolkit/BuddyCommTk.h>

template<typename BaseT, typename LockStateT>
class MirroredMessage : public BaseT
{
   protected:
      typedef MirroredMessage BaseType;

      BuddyResyncJob* resyncJob;
      LockStateT lockState;

      MirroredMessage():
         resyncJob(nullptr)
      {}

      virtual FhgfsOpsErr processSecondaryResponse(NetMessage& resp) = 0;

      virtual const char* mirrorLogContext() const = 0;

      virtual std::unique_ptr<MirroredMessageResponseState> executeLocally(
         NetMessage::ResponseContext& ctx, bool isSecondary) = 0;

      virtual bool isMirrored() = 0;

      // IMPORTANT NOTE ON LOCKING ORDER:
      //  * always take locks the order
      //     - HashDirLock
      //     - DirIDLock
      //     - ParentNameLock
      //     - FileIDLock
      //  * always take locks of each type with the order induced by:
      //     - HashDirLock: id
      //     - DirIDLock: (id, forWrite)
      //     - ParentNameLock: (parentID, name)
      //     - FileIDLock: id
      //
      // not doing this may result in deadlocks.
      virtual LockStateT lock(EntryLockStore& store) = 0;

      virtual void forwardToSecondary(NetMessage::ResponseContext& ctx) = 0;

      virtual bool processIncoming(NetMessage::ResponseContext& ctx)
      {
         Session* session = nullptr;
         bool isNewState = true;

         if (isMirrored() && !this->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
         {
            if (Program::getApp()->getInternodeSyncer()->getResyncInProgress())
               resyncJob = Program::getApp()->getBuddyResyncer()->getResyncJob();

            lockState = lock(*Program::getApp()->getMirroredSessions()->getEntryLockStore());
         }

         // make sure that the thread change set is *always* cleared when we leave this method.
         struct _ClearChangeSet {
            ~_ClearChangeSet()
            {
               if (BuddyResyncer::getSyncChangeset())
               {
                  LOG(MIRRORING, WARNING, "Abandoning sync changeset");
                  BuddyResyncer::abandonSyncChangeset();
               }
            }
         } _clearChangeSet;
         (void) _clearChangeSet;

         mirrorState.reset();
         if (isMirrored())
         {
            const auto nodeID = this->getRequestorID(ctx).second;
            session = Program::getApp()->getMirroredSessions()->referenceSession(nodeID, true);
         }

         if (isMirrored() && this->hasFlag(NetMessageHeader::Flag_HasSequenceNumber))
         {
            // special case: client has not been told where to start its sequence. in this case,
            // we want to answer with only the new seqNoBase for the client, and do NO processing.
            if (this->getSequenceNumber() == 0)
            {
               GenericResponseMsg response(GenericRespMsgCode_NEWSEQNOBASE, "New seqNoBase");

               response.addFlag(NetMessageHeader::Flag_HasSequenceNumber);
               response.setSequenceNumber(session->getSeqNoBase());
               ctx.sendResponse(response);
               goto exit;
            }

            // a note on locking of mirrorState. since clients process each request in only one
            // thread, per client we can have only one request for a given sequence number at any
            // given time. retries may reuse the same sequence number, and they may be processed in
            // a different thread on the server, but no two threads process the same sequence number
            // from the same client at the same time. thus, no locking for the actual structure is
            // needed, but extra memory barriers to ensure propagation of results between threads
            // are necessary.
            __sync_synchronize();
            if (this->hasFlag(NetMessageHeader::Flag_IsSelectiveAck))
               std::tie(mirrorState, isNewState) = session->acquireMirrorStateSlotSelective(
                     this->getSequenceNumberDone(),
                     this->getSequenceNumber());
            else
               std::tie(mirrorState, isNewState) = session->acquireMirrorStateSlot(
                     this->getSequenceNumberDone(),
                     this->getSequenceNumber());
         }

         if (!isNewState)
         {
            if (mirrorState->response)
               mirrorState->response->sendResponse(ctx);
            else
               ctx.sendResponse(
                     GenericResponseMsg(
                        GenericRespMsgCode_TRYAGAIN,
                        "Request for same sequence number is currently in progress"));
         }
         else
         {
            if (resyncJob && resyncJob->isRunning())
	    {
               BuddyResyncer::registerSyncChangeset();
	       resyncJob->registerOps();
	    }

            auto responseState = executeLocally(ctx,
               isMirrored() && this->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond));

            // responseState may ne null if the message has called earlyComplete(). do not finish
            // the operation twice in this case.
            if (responseState)
               finishOperation(ctx, std::move(responseState));
         }

      exit:
         if (session)
            Program::getApp()->getMirroredSessions()->releaseSession(session);

         return true;
      }

      template<typename ResponseT>
      void earlyComplete(NetMessage::ResponseContext& ctx, ResponseT&& state)
      {
         finishOperation(ctx, boost::make_unique<ResponseT>(std::move(state)));

         Socket* sock = ctx.getSocket();
         IncomingPreprocessedMsgWork::releaseSocket(Program::getApp(), &sock, this);
      }

      void buddyResyncNotify(NetMessage::ResponseContext& ctx, bool stateChanged)
      {
         // pairs with the memory barrier before acquireMirrorStateSlot
         __sync_synchronize();

         if (BuddyResyncer::getSyncChangeset())
         {
            if (isMirrored() &&
                  !this->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond) &&
                  stateChanged)
               BuddyResyncer::commitThreadChangeSet();
            else
               BuddyResyncer::abandonSyncChangeset();
         }
      }

      void finishOperation(NetMessage::ResponseContext& ctx,
         std::unique_ptr<MirroredMessageResponseState> state)
      {
         auto* responsePtr = state.get();

         if (isMirrored() &&
               !this->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond) &&
               state)
         {
            if (state->changesObservableState())
               forwardToSecondary(ctx);
            else
               notifySecondaryOfACK(ctx);
         }

         if (mirrorState)
            mirrorState->response = std::move(state);

         // pairs with the memory barrier before acquireMirrorStateSlot
         __sync_synchronize();

         if (BuddyResyncer::getSyncChangeset())
         {
	    resyncJob = Program::getApp()->getBuddyResyncer()->getResyncJob();
            if (isMirrored() &&
                  !this->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond) &&
                  responsePtr &&
                  responsePtr->changesObservableState())
               BuddyResyncer::commitThreadChangeSet();
            else
               BuddyResyncer::abandonSyncChangeset();

	    resyncJob->unregisterOps();
         }

         if (responsePtr)
            responsePtr->sendResponse(ctx);

         lockState = {};
      }

      void notifySecondaryOfACK(NetMessage::ResponseContext& ctx)
      {
         AckNotifiyMsg msg;
         // if the secondary does not respond with SUCCESS, it will automatically be set to
         // needs-resync. eventually, resync will clear the secondary sessions entirely, which will
         // also flush the sequence number store.
         sendToSecondary(ctx, msg, NETMSGTYPE_AckNotifyResp);
      }

      virtual void prepareMirrorRequestArgs(RequestResponseArgs& args)
      {
      }

      template<typename T>
      void sendToSecondary(NetMessage::ResponseContext& ctx, MirroredMessageBase<T>& message,
         unsigned respType, FhgfsOpsErr expectedResult = FhgfsOpsErr_SUCCESS)
      {
         App* app = Program::getApp();
         NodeStoreServers* metaNodes = app->getMetaNodes();
         MirrorBuddyGroupMapper* buddyGroups = app->getMetaBuddyGroupMapper();

         DEBUG_ENV_VAR(unsigned, FORWARD_DELAY, 0, "BEEGFS_FORWARD_DELAY_SECS");

         if (FORWARD_DELAY)
            sleep(FORWARD_DELAY);

         // if a resync is currently running, abort right here, immediatly. we do not need to know
         // the exact state of the buddy: a resync is running. it's bad.
         if (app->getInternodeSyncer()->getResyncInProgress())
            return;

         // check whether the secondary is viable at all: if it is not online and good,
         // communicating will not do any good. even online/needs-resync must be skipped, because
         // the resyncer must be the only entitity that changes the secondary as long as it is not
         // good yet.
         {
            CombinedTargetState secondaryState;
            NumNodeID secondaryID(buddyGroups->getSecondaryTargetID(
                  buddyGroups->getLocalGroupID()));

            bool getStateRes = app->getMetaStateStore()->getState(secondaryID.val(),
                  secondaryState);

            // if the secondary is anything except online/good, set it to needs-resync immediately.
            // whenever we pass this point, the secondary will have missed *something* of
            // importance, so anything except online/good must be set to needs-resync right here.
            if (!getStateRes
                  || secondaryState.reachabilityState != TargetReachabilityState_ONLINE
                  || secondaryState.consistencyState != TargetConsistencyState_GOOD)
            {
               auto* const resyncer = app->getBuddyResyncer();
               auto* const job = resyncer->getResyncJob();

               // if we have no job or a running job, we must start a resync soon. if we have a
               // job that has finished successfully, the management server may not have noticed
               // that the secondary is completely resynced, so our buddys state may well not be
               // GOOD even though we have resynced completely. we may assume that a successful
               // resync implies that the buddy is good, even if the management server thinks it
               // isn't.
               if (!job ||
                     (!job->isRunning() && job->getState() != BuddyResyncJobState_SUCCESS))
               {
                  setBuddyNeedsResync();
                  return;
               }
            }
         }

         RequestResponseArgs rrArgs(NULL, &message, respType);
         RequestResponseNode rrNode(NumNodeID(buddyGroups->getLocalGroupID()), metaNodes);

         rrNode.setMirrorInfo(buddyGroups, true);
         rrNode.setTargetStates(app->getMetaStateStore());

         prepareMirrorRequestArgs(rrArgs);

         // copy sequence numbers and set original requestor info for secondary
         message.setSequenceNumber(this->getSequenceNumber());
         message.setSequenceNumberDone(this->getSequenceNumberDone());
         message.setRequestorID(this->getRequestorID(ctx));
         // (almost) all messages do some sort of statistics gathering by user ID
         message.setMsgHeaderUserID(this->getMsgHeaderUserID());
         // set flag here instead of at the beginning because &message == this is often used
         message.addFlag(NetMessageHeader::Flag_BuddyMirrorSecond);
         message.addFlag(this->getFlags() & NetMessageHeader::Flag_IsSelectiveAck);
         message.addFlag(this->getFlags() & NetMessageHeader::Flag_HasSequenceNumber);

         FhgfsOpsErr commRes = MessagingTk::requestResponseNode(&rrNode, &rrArgs);

         message.removeFlag(NetMessageHeader::Flag_BuddyMirrorSecond);

         if (commRes != FhgfsOpsErr_SUCCESS)
         {
            // since we have reached this point, the secondary has indubitably not received
            // important information from the primary. we now have two choices to keep the system
            // in a consistent, safe state:
            //
            //  1) set the secondary to needs-resync
            //  2) rollback the modifications we have made and let the client retry, hoping that
            //     some future communication with the secondary is successful
            //
            // 2 is not a viable option: since some operations may move data off of this metadata
            // server and onto another one completely; allowing these to be undone requires a
            // two-phase commit protocol, which incurs large communication overhead for a
            // (hopefully) very rare error case. other operations delete local state (eg unlink,
            // or close of an unlinked file), which would have to be held in limbo until either a
            // commit or a rollback is issued.
            //
            // since we assume that communication errors are very rare, option 1 is the most
            // efficient in the general case (as it does not have to keep objects alive past their
            // intended lifetimes), so we set the secondary to needs-resync on any kind of
            // communication error.
            // other errors, e.g. out-of-memory conditions or errors caused by streamout hooks, are
            // also assumed to be rare. if any of these happens, the secondary must be resynced no
            // matter what actually happened. since the operations itself succeeded, we cannot send
            // a notification about the communication error either - we'd have to drop the operation
            // result to do that.

#ifdef BEEGFS_DEBUG
            int buddyNodeID = buddyGroups->getBuddyTargetID(app->getLocalNodeNumID().val());

            LOG_CTX(MIRRORING, DEBUG, mirrorLogContext(), "Communication with secondary failed. "
                  "Resync will be required when secondary comes back", buddyNodeID, commRes);
#endif
            setBuddyNeedsResync();

            return;
         }

         FhgfsOpsErr respMsgRes = processSecondaryResponse(*rrArgs.outRespMsg);

         if (respMsgRes != expectedResult)
         {
            // whoops; primary and secondary did different things; if secondary is not resyncing
            // AND communication was good this is concerning (result must have been success on
            // primary, otherwise no forwarding would have happened).
            // usually, this would mean that primary and secondary do not have the same state, or
            // that the secondary has some kind of system error. (if the primary had a system error,
            // it would be more likely to fail than to succeed).
            // in either case, the secondary should be resynced, even if the primary experienced
            // a hardware fault or similar errors: at this point, we can no longer differentiate
            // between good and bad state on the primary, and the secondary may be arbitrarily out
            // of sync.
            LOG_CTX(MIRRORING, NOTICE, mirrorLogContext(),
                  "Different return codes from primary and secondary buddy. "
                  "Setting secondary to needs-resync.",
                  ("Expected response", expectedResult),
                  ("Received response", respMsgRes));
            setBuddyNeedsResync();
         }
      }

      // inodes that are changes during mirrored processing on the secondary (eg file creation or
      // deletion, setxattr, etc) may have timestamps changes to a different value than the primary.
      // to remedy this, the secondary must explicitly set these timestamps during processing.
      bool shouldFixTimestamps()
      {
         return isMirrored() && Program::getApp()->getConfig()->getTuneMirrorTimestamps();
      }

      void fixInodeTimestamp(DirInode& inode, MirroredTimestamps& ts)
      {
         if (!isMirrored())
            return;

         BEEGFS_BUG_ON_DEBUG(!inode.getIsLoaded(), "inode not loaded");

         StatData stat;

         inode.getStatData(stat);

         if (!this->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
         {
            ts = stat.getMirroredTimestamps();
         }
         else
         {
            stat.setMirroredTimestamps(ts);

            inode.setStatData(stat);
         }
      }

      void fixInodeTimestamp(FileInode& inode, MirroredTimestamps& ts,
         EntryInfo* const saveEntryInfo)
      {
         if (!isMirrored())
            return;

         StatData stat;

         inode.getStatData(stat);

         if (!this->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond))
         {
            ts = stat.getMirroredTimestamps();
         }
         else
         {
            stat.setMirroredTimestamps(ts);

            inode.setStatData(stat);
            if (saveEntryInfo)
               inode.updateInodeOnDisk(saveEntryInfo);
         }
      }

      void updateNodeOp(NetMessage::ResponseContext& ctx, MetaOpCounterTypes type)
      {
         const auto counter = isMirrored() && this->hasFlag(NetMessageHeader::Flag_BuddyMirrorSecond)
            ? MetaOpCounter_MIRROR
            : type;

         Program::getApp()->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(),
               counter, this->getMsgHeaderUserID());
      }

   private:
      std::shared_ptr<MirrorStateSlot> mirrorState;

      void setBuddyNeedsResync()
      {
         BuddyCommTk::setBuddyNeedsResync(Program::getApp()->getMetaPath(), true);
      }
};

#endif
