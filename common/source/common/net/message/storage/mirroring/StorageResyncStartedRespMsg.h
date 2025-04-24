#pragma once

#include <common/net/message/SimpleMsg.h>

class StorageResyncStartedRespMsg : public SimpleMsg
{
   public:
      StorageResyncStartedRespMsg() : SimpleMsg(NETMSGTYPE_StorageResyncStartedResp)
      {
      }
};

