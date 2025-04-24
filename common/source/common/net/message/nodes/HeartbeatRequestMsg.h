#pragma once

#include <common/Common.h>
#include "../SimpleMsg.h"

class HeartbeatRequestMsg : public SimpleMsg
{
   public:
      HeartbeatRequestMsg() : SimpleMsg(NETMSGTYPE_HeartbeatRequest)
      {
      }
};


