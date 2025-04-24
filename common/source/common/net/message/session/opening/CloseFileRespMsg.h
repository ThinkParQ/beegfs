#pragma once

#include <common/net/message/SimpleIntMsg.h>

class CloseFileRespMsg : public SimpleIntMsg
{
   public:
      CloseFileRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_CloseFileResp, result)
      {
      }

      CloseFileRespMsg() : SimpleIntMsg(NETMSGTYPE_CloseFileResp)
      {
      }
};

