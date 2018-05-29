#include "MoveChunkFileMsgEx.h"

#include <program/Program.h>

bool MoveChunkFileMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext = "MoveChunkFileMsg incoming";

#ifdef BEEGFS_DEBUG
   LOG_DEBUG(logContext, 4, "Received a MoveChunkFileMsg from: " + ctx.peerName() );
#endif // BEEGFS_DEBUG

   unsigned result = 0;

   App* app = Program::getApp();

   std::string chunkName = this->getChunkName();
   std::string oldPath = this->getOldPath(); // relative path to chunks dir
   std::string newPath = this->getNewPath(); // relative path to chunks dir
   uint16_t targetID = this->getTargetID();
   bool overwriteExisting = this->getOverwriteExisting();

   std::string targetPath;
   int renameRes;

   app->getStorageTargets()->getPath(targetID, &targetPath);

   if (getIsMirrored()) // it's a mirror chunk
   {
      // add mirror component to relative path
      targetPath += "/" CONFIG_BUDDYMIRROR_SUBDIR_NAME;
   }
   else
   {
      targetPath += "/" CONFIG_CHUNK_SUBDIR_NAME;
   }

   std::string moveFrom = oldPath + "/" + chunkName;
   std::string moveTo = newPath + "/" + chunkName;

   int targetFD = app->getTargetFD(targetID, getIsMirrored());

   if ( unlikely(targetFD == -1) )
   {
      LogContext(logContext).log(Log_CRITICAL, "Could not open path for target ID; targetID: "
         + StringTk::uintToStr(targetID));
      result = 1;
      goto sendResp;
   }

   // if overwriteExisting set to false, make sure, that output file does not exist
   if (!overwriteExisting)
   {
      bool pathExists = StorageTk::pathExists(targetFD, moveTo);
      if (pathExists)
      {
         LogContext(logContext).log(Log_CRITICAL,
            "Could not move chunk file. Destination file does already exist; chunkID: " + chunkName
               + "; targetID: " + StringTk::uintToStr(targetID) + "; oldChunkPath: " + oldPath
               + "; newChunkPath: " + newPath);
         result = 1;
         goto sendResp;
      }
   }

   {
      // create the parent directory (perhaps it didn't exist)
      // can be more efficient if we write a createPathOnDisk that uses mkdirat
      Path moveToPath = Path(targetPath + "/" + moveTo);
      mode_t dirMode = S_IRWXU | S_IRWXG | S_IRWXO;
      bool mkdirRes = StorageTk::createPathOnDisk(moveToPath, true, &dirMode);

      if(!mkdirRes)
      {
         LogContext(logContext).log(Log_CRITICAL,
            "Could not create parent directory for chunk; chunkID: " + chunkName + "; targetID: "
               + StringTk::uintToStr(targetID) + "; oldChunkPath: " + oldPath + "; newChunkPath: "
               + newPath);
         result = 1;
         goto sendResp;
      }
   }

   // perform the actual move
   renameRes = renameat(targetFD, moveFrom.c_str(), targetFD, moveTo.c_str() );
   if ( renameRes != 0 )
   {
      LogContext(logContext).log(Log_CRITICAL,
         "Could not perform move; chunkID: " + chunkName + "; targetID: "
            + StringTk::uintToStr(targetID) + "; oldChunkPath: " + oldPath + "; newChunkPath: "
            + newPath + "; SysErr: " + System::getErrString());
      result = 1;
   }
   else if (getIsMirrored())
   {
      auto* buddyGroups = app->getMirrorBuddyGroupMapper();
      auto buddyGroupID = buddyGroups->getBuddyGroupID(targetID);
      auto secondaryTargetID = buddyGroups->getSecondaryTargetID(buddyGroupID);
      app->getStorageTargets()->setBuddyNeedsResync(targetID, true, secondaryTargetID);
   }

sendResp:
   ctx.sendResponse(MoveChunkFileRespMsg(result) );

   return true;
}
