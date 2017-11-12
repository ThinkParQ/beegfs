#include <common/net/message/storage/creating/RmChunkPathsRespMsg.h>
#include <toolkit/StorageTkEx.h>
#include <program/Program.h>

#include "RmChunkPathsMsgEx.h"

bool RmChunkPathsMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "RmChunkPathsMsg incoming";

   #ifdef BEEGFS_DEBUG
      LOG_DEBUG(logContext, Log_DEBUG, "Received a RmChunkPathsMsg from: " + ctx.peerName() );
   #endif // BEEGFS_DEBUG

   App* app = Program::getApp();
   ChunkStore* chunkStore = app->getChunkDirStore();

   uint16_t targetID;
   StringList& relativePaths = getRelativePaths();
   StringList failedPaths;

   targetID = getTargetID();

   // get targetFD
   int targetFD = app->getTargetFD(targetID,
      isMsgHeaderFeatureFlagSet(RMCHUNKPATHSMSG_FLAG_BUDDYMIRROR));
   if(unlikely(targetFD == -1))
   { // unknown targetID
      LogContext(logContext).logErr("Unknown targetID: " + StringTk::uintToStr(targetID));
      failedPaths = relativePaths;
   }
   else
   { // valid targetID
      for(StringListIter iter = relativePaths.begin(); iter != relativePaths.end(); iter++)
      {
         // remove chunk
         int unlinkRes = unlinkat(targetFD, (*iter).c_str(), 0);

         if ( (unlinkRes != 0)  && (errno != ENOENT) )
         {
            LogContext(logContext).logErr(
               "Unable to remove entry: " + *iter + "; error: " + System::getErrString());
            failedPaths.push_back(*iter);

            continue;
         }

         // removal succeeded; this might have been the last entry => try to remove parent directory
         Path parentDirPath(StorageTk::getPathDirname(*iter));

         chunkStore->rmdirChunkDirPath(targetFD, &parentDirPath);
      }
   }

   ctx.sendResponse(RmChunkPathsRespMsg(&failedPaths) );

   return true;
}

