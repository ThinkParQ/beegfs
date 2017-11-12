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
   bool commRes;
   char* respBuf = NULL;
   NetMessage* respMsg = NULL;
   StatStoragePathRespMsg* respMsgCast;

   StatStoragePathMsg msg(targetID);

   // connect & communicate
   commRes = MessagingTk::requestResponse(
      node, &msg, NETMSGTYPE_StatStoragePathResp, &respBuf, &respMsg);
   if(!commRes)
      return FhgfsOpsErr_COMMUNICATION;

   // handle result
   respMsgCast = (StatStoragePathRespMsg*)respMsg;

   retVal = (FhgfsOpsErr)respMsgCast->getResult();

   *outFree = respMsgCast->getSizeFree();
   *outTotal = respMsgCast->getSizeTotal();
   *outInodesFree = respMsgCast->getInodesFree();
   *outInodesTotal = respMsgCast->getInodesTotal();

   // cleanup
   SAFE_DELETE(respMsg);
   SAFE_FREE(respBuf);

   return retVal;
}
