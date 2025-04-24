#pragma once

#include <common/net/message/SimpleIntMsg.h>

class UnlinkFileRespMsg : public SimpleIntMsg
{
   public:
      UnlinkFileRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_UnlinkFileResp, result)
      {
      }

      UnlinkFileRespMsg() : SimpleIntMsg(NETMSGTYPE_UnlinkFileResp)
      {
      }
};

