#ifndef REMOVEINODERESPMSG_H
#define REMOVEINODERESPMSG_H

#include <common/net/message/SimpleIntMsg.h>
#include <common/storage/StorageErrors.h>

class RemoveInodeRespMsg : public SimpleIntMsg
{
   public:
      RemoveInodeRespMsg(FhgfsOpsErr result) : SimpleIntMsg(NETMSGTYPE_RemoveInodeResp, (int)result)
      {
      }

      RemoveInodeRespMsg() : SimpleIntMsg(NETMSGTYPE_RemoveInodeResp)
      {
      }

      FhgfsOpsErr getResult()
      {
         return (FhgfsOpsErr)getValue();
      }
};

#endif /*REMOVEINODERESPMSG_H*/
