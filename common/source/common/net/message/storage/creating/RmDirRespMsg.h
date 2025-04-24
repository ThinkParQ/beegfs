#pragma once

#include <common/net/message/SimpleIntMsg.h>

class RmDirRespMsg : public SimpleIntMsg
{
   public:
      RmDirRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_RmDirResp, result)
      {
      }

      RmDirRespMsg() : SimpleIntMsg(NETMSGTYPE_RmDirResp)
      {
      }
};

