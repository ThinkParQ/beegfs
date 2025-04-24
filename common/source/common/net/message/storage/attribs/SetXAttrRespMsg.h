#pragma once

#include <common/net/message/SimpleIntMsg.h>

class SetXAttrRespMsg : public SimpleIntMsg
{
   public:
      SetXAttrRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_SetXAttrResp, result) {}

      SetXAttrRespMsg() : SimpleIntMsg(NETMSGTYPE_SetXAttrResp) {}
};

