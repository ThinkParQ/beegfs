#pragma once

#include <common/net/message/SimpleIntMsg.h>

class UpdateDirParentRespMsg : public SimpleIntMsg
{
   public:
      UpdateDirParentRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_UpdateDirParentResp, result)
      {
      }

      UpdateDirParentRespMsg() : SimpleIntMsg(NETMSGTYPE_UpdateDirParentResp)
      {
      }
};

