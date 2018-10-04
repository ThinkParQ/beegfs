#include <program/Program.h>
#include <common/net/message/storage/StatStoragePathRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include "StatStoragePathMsgEx.h"


bool StatStoragePathMsgEx::processIncoming(ResponseContext& ctx)
{
   int64_t sizeTotal = 0;
   int64_t sizeFree = 0;
   int64_t inodesTotal = 0;
   int64_t inodesFree = 0;

   FhgfsOpsErr statRes = statStoragePath(&sizeTotal, &sizeFree, &inodesTotal, &inodesFree);

   ctx.sendResponse(StatStoragePathRespMsg(statRes, sizeTotal, sizeFree, inodesTotal, inodesFree) );

   App* app = Program::getApp();
   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(),
      StorageOpCounter_STATSTORAGEPATH, getMsgHeaderUserID() );

   return true;
}

FhgfsOpsErr StatStoragePathMsgEx::statStoragePath(int64_t* outSizeTotal, int64_t* outSizeFree,
   int64_t* outInodesTotal, int64_t* outInodesFree)
{
   const char* logContext = "StatStoragePathMsg (stat path)";

   App* app = Program::getApp();

   auto* const target = app->getStorageTargets()->getTarget(getTargetID());
   if (!target)
   {
      LogContext(logContext).logErr("Unknown targetID: " + StringTk::uintToStr(getTargetID() ) );
      return FhgfsOpsErr_UNKNOWNTARGET;
   }

   const auto& targetPath = target->getPath().str();

   bool statSuccess = StorageTk::statStoragePath(targetPath, outSizeTotal, outSizeFree,
      outInodesTotal, outInodesFree);
   if(unlikely(!statSuccess) )
   { // error
      LogContext(logContext).logErr("Unable to statfs() storage path: " + targetPath +
         " (SysErr: " + System::getErrString() );

      return FhgfsOpsErr_INTERNAL;
   }

   // read and use value from manual free space override file (if it exists)
   StorageTk::statStoragePathOverride(targetPath, outSizeFree, outInodesFree);

   return FhgfsOpsErr_SUCCESS;
}

