#pragma once

#include <common/net/message/SimpleIntMsg.h>

class LogRespMsg : public SimpleIntMsg
{
   public:
      LogRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_LogResp, result)
      {
      }

      LogRespMsg() : SimpleIntMsg(NETMSGTYPE_LogResp)
      {
      }
};

