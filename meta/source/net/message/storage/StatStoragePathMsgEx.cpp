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
   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), MetaOpCounter_STATFS,
      getMsgHeaderUserID() );

   return true;
}

FhgfsOpsErr StatStoragePathMsgEx::statStoragePath(int64_t* outSizeTotal, int64_t* outSizeFree,
   int64_t* outInodesTotal, int64_t* outInodesFree)
{
   const char* logContext = "StatStoragePathMsg (stat path)";

   std::string pathStr = Program::getApp()->getMetaPath();

   bool statSuccess = StorageTk::statStoragePath(
      pathStr, outSizeTotal, outSizeFree, outInodesTotal, outInodesFree);

   if(unlikely(!statSuccess) )
   { // error
      LogContext(logContext).logErr("Unable to statfs() storage path: " + pathStr +
         " (SysErr: " + System::getErrString() );

      return FhgfsOpsErr_INTERNAL;
   }


   StorageTk::statStoragePathOverride(pathStr, outSizeFree, outInodesFree);

   return FhgfsOpsErr_SUCCESS;
}


