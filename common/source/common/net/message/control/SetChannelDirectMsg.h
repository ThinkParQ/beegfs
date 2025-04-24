#pragma once

#include <common/net/message/SimpleIntMsg.h>

class SetChannelDirectMsg : public SimpleIntMsg
{
   public:
      SetChannelDirectMsg(int isDirect) : SimpleIntMsg(NETMSGTYPE_SetChannelDirect, isDirect)
      {
      }

      /**
       * For deserialization only
       */
      SetChannelDirectMsg() : SimpleIntMsg(NETMSGTYPE_SetChannelDirect)
      {
      }
};

