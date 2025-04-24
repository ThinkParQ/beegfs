#pragma once

#include <common/net/message/SimpleIntMsg.h>

class RenameRespMsg : public SimpleIntMsg
{
   public:
      RenameRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_RenameResp, result)
      {
      }

      RenameRespMsg() : SimpleIntMsg(NETMSGTYPE_RenameResp)
      {
      }
};


