#pragma once

#include <common/net/message/SimpleIntMsg.h>

class SetAttrRespMsg : public SimpleIntMsg
{
   public:
      SetAttrRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_SetAttrResp, result)
      {
      }

      SetAttrRespMsg() : SimpleIntMsg(NETMSGTYPE_SetAttrResp)
      {
      }
};

