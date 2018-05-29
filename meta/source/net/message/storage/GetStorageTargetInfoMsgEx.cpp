#include <common/components/AbstractDatagramListener.h>
#include <common/net/message/storage/GetStorageTargetInfoRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <components/InternodeSyncer.h>
#include <program/Program.h>
#include "GetStorageTargetInfoMsgEx.h"


bool GetStorageTargetInfoMsgEx::processIncoming(ResponseContext& ctx)
{
   const char* logContext("GetStorageTargetInfoMsg incoming");

   LOG_DEBUG(logContext, Log_DEBUG, "Received a GetStorageTargetInfoMsg from: " + ctx.peerName() );

   App* app = Program::getApp();

   const UInt16List& targetIDs = getTargetIDs();
   StorageTargetInfoList targetInfoList;

   std::string targetPathStr;
   int64_t sizeTotal = 0;
   int64_t sizeFree = 0;
   int64_t inodesTotal = 0;
   int64_t inodesFree = 0;

   TargetConsistencyState nodeState = TargetConsistencyState_GOOD;

   // (note: empty targetIDs means "get all targetIDs")
   if(!targetIDs.empty() && (*targetIDs.begin() != app->getLocalNodeNumID().val() ) )
   { // invalid nodeID given
      LogContext(logContext).logErr(
         "Unknown targetID: " + StringTk::uintToStr(*targetIDs.begin() ) );

      // Report default initialized target state (OFFLINE/GOOD).
   }
   else
   {
      targetPathStr = app->getMetaPath();

      getStatInfo(&sizeTotal, &sizeFree, &inodesTotal, &inodesFree, nodeState);
   }


   StorageTargetInfo targetInfo(app->getLocalNodeNumID().val(), targetPathStr, sizeTotal, sizeFree,
      inodesTotal, inodesFree, nodeState);
   targetInfoList.push_back(targetInfo);

   ctx.sendResponse(GetStorageTargetInfoRespMsg(&targetInfoList) );

   // update stats

   app->getNodeOpStats()->updateNodeOp(ctx.getSocket()->getPeerIP(), MetaOpCounter_STATFS,
      getMsgHeaderUserID() );

   return true;
}

void GetStorageTargetInfoMsgEx::getStatInfo(int64_t* outSizeTotal, int64_t* outSizeFree,
   int64_t* outInodesTotal, int64_t* outInodesFree, TargetConsistencyState& nodeState)
{
   const char* logContext = "GetStorageTargetInfoMsg (stat path)";

   InternodeSyncer* internodeSyncer = Program::getApp()->getInternodeSyncer();

   std::string targetPathStr = Program::getApp()->getMetaPath();

   bool statSuccess = StorageTk::statStoragePath(targetPathStr, outSizeTotal, outSizeFree,
      outInodesTotal, outInodesFree);

   if(unlikely(!statSuccess) )
   { // error
      LogContext(logContext).logErr("Unable to statfs() storage path: " + targetPathStr +
         " (SysErr: " + System::getErrString() );

      *outSizeTotal = -1;
      *outSizeFree = -1;
      *outInodesTotal = -1;
      *outInodesFree = -1;
   }

   // read and use value from manual free space override file (if it exists)
   StorageTk::statStoragePathOverride(targetPathStr, outSizeFree, outInodesFree);

   nodeState = internodeSyncer->getNodeConsistencyState();
}
