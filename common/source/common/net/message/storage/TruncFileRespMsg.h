#pragma once

#include <common/net/message/SimpleIntMsg.h>


class TruncFileRespMsg : public SimpleIntMsg
{
   public:
      TruncFileRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_TruncFileResp, result)
      {
      }

      TruncFileRespMsg() : SimpleIntMsg(NETMSGTYPE_TruncFileResp)
      {
      }
};

