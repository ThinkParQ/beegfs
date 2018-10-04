#include "StorageTargetInfo.h"

#include <common/net/message/storage/StatStoragePathMsg.h>
#include <common/net/message/storage/StatStoragePathRespMsg.h>
#include <common/toolkit/MessagingTk.h>
#include <common/toolkit/serialization/Serialization.h>


/**
 * @param targetID only required for storage servers, leave empty otherwise.
 * @return false on comm error, true otherwise.
 */
FhgfsOpsErr StorageTargetInfo::statStoragePath(Node& node, uint16_t targetID, int64_t* outFree,
   int64_t* outTotal, int64_t* outInodesFree, int64_t* outInodesTotal)
{
   FhgfsOpsErr retVal;
   StatStoragePathRespMsg* respMsgCast;

   StatStoragePathMsg msg(targetID);

   const auto respMsg = MessagingTk::requestResponse(node, msg, NETMSGTYPE_StatStoragePathResp);
   if (!respMsg)
      return FhgfsOpsErr_COMMUNICATION;

   // handle result
   respMsgCast = (StatStoragePathRespMsg*)respMsg.get();

   retVal = (FhgfsOpsErr)respMsgCast->getResult();

   *outFree = respMsgCast->getSizeFree();
   *outTotal = respMsgCast->getSizeTotal();
   *outInodesFree = respMsgCast->getInodesFree();
   *outInodesTotal = respMsgCast->getInodesTotal();

   return retVal;
}
