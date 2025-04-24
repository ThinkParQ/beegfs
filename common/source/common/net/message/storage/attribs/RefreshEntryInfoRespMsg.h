#pragma once

#include <common/net/message/SimpleIntMsg.h>

class RefreshEntryInfoRespMsg : public SimpleIntMsg
{
   public:
      RefreshEntryInfoRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_RefreshEntryInfoResp, result)
      {
      }

      RefreshEntryInfoRespMsg() : SimpleIntMsg(NETMSGTYPE_RefreshEntryInfoResp)
      {
      }
};

