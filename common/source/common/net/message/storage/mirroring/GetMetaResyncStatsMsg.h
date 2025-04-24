#pragma once

#include <common/net/message/SimpleMsg.h>

class GetMetaResyncStatsMsg : public SimpleMsg
{
   public:
      GetMetaResyncStatsMsg() : SimpleMsg(NETMSGTYPE_GetMetaResyncStats) { }
};

