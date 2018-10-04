#include "MoveChunkFileMsgEx.h"

#include <program/Program.h>

bool MoveChunkFileMsgEx::processIncoming(ResponseContext& ctx)
{
   ctx.sendResponse(MoveChunkFileRespMsg(moveChunk()));
   return true;
}

unsigned MoveChunkFileMsgEx::moveChunk()
{
   const char* logContext = "MoveChunkFileMsg incoming";

   App* app = Program::getApp();

   std::string chunkName = this->getChunkName();
   std::string oldPath = this->getOldPath(); // relative path to chunks dir
   std::string newPath = this->getNewPath(); // relative path to chunks dir
   uint16_t targetID = this->getTargetID();
   bool overwriteExisting = this->getOverwriteExisting();

   int renameRes;

   std::string moveFrom = oldPath + "/" + chunkName;
   std::string moveTo = newPath + "/" + chunkName;

   auto* const target = app->getStorageTargets()->getTarget(targetID);

   if (!target)
   {
      LogContext(logContext).log(Log_CRITICAL, "Could not open path for target ID; targetID: "
         + StringTk::uintToStr(targetID));
      return 1;
   }

   const auto targetPath = getIsMirrored()
      ? target->getPath() / CONFIG_BUDDYMIRROR_SUBDIR_NAME
      : target->getPath() / CONFIG_CHUNK_SUBDIR_NAME;

   const int targetFD = getIsMirrored() ? *target->getMirrorFD() : *target->getChunkFD();

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
         return 1;
      }
   }

   {
      // create the parent directory (perhaps it didn't exist)
      // can be more efficient if we write a createPathOnDisk that uses mkdirat
      const Path moveToPath = targetPath / moveTo;
      mode_t dirMode = S_IRWXU | S_IRWXG | S_IRWXO;
      bool mkdirRes = StorageTk::createPathOnDisk(moveToPath, true, &dirMode);

      if(!mkdirRes)
      {
         LogContext(logContext).log(Log_CRITICAL,
            "Could not create parent directory for chunk; chunkID: " + chunkName + "; targetID: "
               + StringTk::uintToStr(targetID) + "; oldChunkPath: " + oldPath + "; newChunkPath: "
               + newPath);
         return 1;
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
      return 1;
   }
   else if (getIsMirrored())
      target->setBuddyNeedsResync(true);

   return 0;
}
