#pragma once

#include <common/net/message/SimpleMsg.h>

class DummyMsg : public SimpleMsg
{
   public:
      DummyMsg() : SimpleMsg(NETMSGTYPE_Dummy)
      {
      }
};


