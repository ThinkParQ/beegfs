#include "DeleteChunksMsgEx.h"

#include <program/Program.h>
#include <toolkit/StorageTkEx.h>

bool DeleteChunksMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "DeleteChunksMsg incoming";

#ifdef BEEGFS_DEBUG
   LOG_DEBUG(logContext, 4, "Received a DeleteChunksMsg from: " + ctx.peerName() );
#endif // BEEGFS_DEBUG

   App* app = Program::getApp();
   ChunkStore* chunkDirStore = app->getChunkDirStore();

   FsckChunkList& chunks = getChunks();
   FsckChunkList failedDeletes;

   for ( FsckChunkListIter iter = chunks.begin(); iter != chunks.end(); iter++ )
   {
      std::string chunkDirRelative;
      std::string delPathStrRelative;
      bool isMirrorFD;

      if (iter->getBuddyGroupID()) // it's a mirror chunk
         isMirrorFD = true;
      else
         isMirrorFD = false;

      chunkDirRelative = iter->getSavedPath()->str();

      delPathStrRelative = chunkDirRelative + "/" + iter->getID();

      int targetFD = app->getTargetFD(iter->getTargetID(), isMirrorFD);

      if ( unlikely(targetFD == -1) )
      { // unknown targetID
         LogContext(logContext).logErr(std::string("Unknown targetID: ") +
            StringTk::uintToStr(iter->getTargetID()));
         failedDeletes.push_back(*iter);
      }
      else
      { // valid targetID
         int unlinkRes = unlinkat(targetFD, delPathStrRelative.c_str(), 0);
         if ( (unlinkRes == -1) && (errno != ENOENT) )
         { // error
            LogContext(logContext).logErr(
               "Unable to unlink file: " + delPathStrRelative + ". " + "SysErr: "
                  + System::getErrString());

            failedDeletes.push_back(*iter);
         }

         // Now try to rmdir chunkDirPath (checks if it is empty)
         if (unlinkRes == 0)
         {
            Path chunkDirRelativeVec(chunkDirRelative);
            chunkDirStore->rmdirChunkDirPath(targetFD, &chunkDirRelativeVec);
         }

      }
   }

   ctx.sendResponse(DeleteChunksRespMsg(&failedDeletes) );

   return true;
}
