#pragma once

#include <common/net/message/SimpleIntMsg.h>

class RemoveXAttrRespMsg : public SimpleIntMsg
{
   public:
      RemoveXAttrRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_RemoveXAttrResp, result) {}

      RemoveXAttrRespMsg() : SimpleIntMsg(NETMSGTYPE_RemoveXAttrResp) {}
};

