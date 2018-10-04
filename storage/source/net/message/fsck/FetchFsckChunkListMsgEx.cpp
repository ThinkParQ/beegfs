#include "FetchFsckChunkListMsgEx.h"

#include <program/Program.h>

bool FetchFsckChunkListMsgEx::processIncoming(ResponseContext& ctx)
{
   App* app = Program::getApp();
   ChunkFetcher* chunkFetcher = app->getChunkFetcher();

   FetchFsckChunkListStatus status;
   FsckChunkList chunkList;

   if (getLastStatus() == FetchFsckChunkListStatus_NOTSTARTED)
   {
      // This is the first message of a new Fsck run
      if (chunkFetcher->getNumRunning() != 0 || !chunkFetcher->isQueueEmpty())
      {
         // another fsck is already in progress
         if (!getForceRestart())
         {
            LOG(GENERAL, NOTICE, "Received request to start fsck although previous run is not finished. "
                  "Not starting.", ("From", ctx.peerName()));

            ctx.sendResponse(FetchFsckChunkListRespMsg(&chunkList,
                     FetchFsckChunkListStatus_NOTSTARTED));
            return true;
         }
         else
         {
            LOG(GENERAL, NOTICE, "Aborting previous fsck chunk fetcher run by user request.",
                  ("From", ctx.peerName()));

            chunkFetcher->stopFetching();
            chunkFetcher->waitForStopFetching();
         }
      }

      chunkFetcher->startFetching();
   }


   if(chunkFetcher->getIsBad())
      status = FetchFsckChunkListStatus_READERROR;
   else if (chunkFetcher->getNumRunning() == 0)
      status = FetchFsckChunkListStatus_FINISHED;
   else
      status = FetchFsckChunkListStatus_RUNNING;

   chunkFetcher->getAndDeleteChunks(chunkList, getMaxNumChunks());

   ctx.sendResponse(FetchFsckChunkListRespMsg(&chunkList, status));
   return true;
}
