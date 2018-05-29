#ifndef SETSTORAGETARGETINFORESPMSG_H_
#define SETSTORAGETARGETINFORESPMSG_H_

#include <common/Common.h>
#include <common/net/message/SimpleIntMsg.h>

class SetStorageTargetInfoRespMsg : public SimpleIntMsg
{
   public:
      SetStorageTargetInfoRespMsg(int result)
         : SimpleIntMsg(NETMSGTYPE_SetStorageTargetInfoResp, result)
      {
      }

      SetStorageTargetInfoRespMsg() : SimpleIntMsg(NETMSGTYPE_SetStorageTargetInfoResp)
      {
      }

      FhgfsOpsErr getResult()
      {
         return (FhgfsOpsErr)getValue();
      }
};

#endif /*SETSTORAGETARGETINFORESPMSG_H_*/
