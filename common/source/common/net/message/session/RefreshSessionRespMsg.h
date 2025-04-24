#pragma once

#include <common/net/message/SimpleIntMsg.h>

class RefreshSessionRespMsg : public SimpleIntMsg
{
   public:
      RefreshSessionRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_RefreshSessionResp, result)
      {
      }

      RefreshSessionRespMsg() : SimpleIntMsg(NETMSGTYPE_RefreshSessionResp)
      {
      }
};

