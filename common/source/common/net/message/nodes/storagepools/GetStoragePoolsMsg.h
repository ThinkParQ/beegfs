#pragma once

#include <common/net/message/SimpleMsg.h>

class GetStoragePoolsMsg : public SimpleMsg
{
   public:
      GetStoragePoolsMsg():
         SimpleMsg(NETMSGTYPE_GetStoragePools) { }
};


