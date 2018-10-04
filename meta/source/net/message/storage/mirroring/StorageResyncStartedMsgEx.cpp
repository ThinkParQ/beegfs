#include <common/components/worker/queue/MultiWorkQueue.h>
#include <common/net/message/storage/mirroring/StorageResyncStartedRespMsg.h>
#include <common/threading/Barrier.h>
#include <common/threading/PThread.h>
#include <common/toolkit/MessagingTk.h>

#include <program/Program.h>
#include <components/worker/BarrierWork.h>

#include "StorageResyncStartedMsgEx.h"

bool StorageResyncStartedMsgEx::processIncoming(ResponseContext& ctx)
{
   NumNodeID nodeID = Program::getApp()->getLocalNodeNumID();

   uint16_t targetID = getValue();
   if (targetID != nodeID.val())
      return false;

   // Make sure all workers have processed all messages that were received before this one.
   // This ensures that no mirrored messages are in flight while resync starts.
   pauseWorkers();

   // we may not have received a heartbeat from the mgmtd yet that could have told us that the
   // root inode is mirrored. since un-mirroring the root inode is currently not possible, we may
   // assume that the root inode is very much supposed to be mirrored.
   // we only need this info if we are currently the secondary of the root buddy group, but we
   // can safely set it on all nodes until we add an option to disable meta mirroring for the
   // root inode again.
   Program::getApp()->getRootDir()->setIsBuddyMirroredFlag(true);

   // clear session store here to get rid of all inodes that will go away during resync.
   Program::getApp()->getMirroredSessions()->clear();

   // we can now be sure that no mirrored dir inode is still referenced by an operation:
   // * the session store is cleared, so no references from there
   // * we have received this message, so must be in NeedsResync state
   //   -> no mirrored operations will be addressed to this node
   // * we hold no references ourselves
   //
   // since resync changes mirrored dir inodes, we must invalidate all inodes that are currently
   // loaded (which will be at least root and mdisposal, plus any cached dir inodes) to ensure that
   // future operations will use the correct resynced data
   Program::getApp()->getMetaStore()->invalidateMirroredDirInodes();

   ctx.sendResponse(StorageResyncStartedRespMsg());

   return true;
}

void StorageResyncStartedMsgEx::pauseWorkers()
{
   App* app = Program::getApp();
   WorkerList* workers = app->getWorkers();
   MultiWorkQueue* workQueue = app->getWorkQueue();
   pthread_t threadID = PThread::getCurrentThreadID();

   // Stop all worker threads except our own
   Barrier workerBarrier(workers->size());
   for (WorkerListIter workerIt = workers->begin(); workerIt != workers->end(); ++workerIt)
   {
      // don't enqueue it in the worker that processes this message (this would deadlock)
      if (!PThread::threadIDEquals((*workerIt)->getID(), threadID))
      {
         PersonalWorkQueue* personalQ = (*workerIt)->getPersonalWorkQueue();
         workQueue->addPersonalWork(new BarrierWork(&workerBarrier), personalQ);
      }
   }

   // Stall our own worker until all the other threads are blocked.
   workerBarrier.wait();

   // Continue all workers
   workerBarrier.wait();
}
