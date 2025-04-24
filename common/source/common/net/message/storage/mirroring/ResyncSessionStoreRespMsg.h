#pragma once

#include <common/net/message/SimpleIntMsg.h>

class ResyncSessionStoreRespMsg : public SimpleIntMsg
{
   public:
      ResyncSessionStoreRespMsg(FhgfsOpsErr result) :
         SimpleIntMsg(NETMSGTYPE_ResyncSessionStoreResp, result) { }

      ResyncSessionStoreRespMsg() : SimpleIntMsg(NETMSGTYPE_ResyncSessionStoreResp) { }

      FhgfsOpsErr getResult()
      {
         return (FhgfsOpsErr)getValue();
      }
};


