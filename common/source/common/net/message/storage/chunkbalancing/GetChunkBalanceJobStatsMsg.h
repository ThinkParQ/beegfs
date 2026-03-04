#pragma once

#include <common/net/message/SimpleMsg.h>

class GetChunkBalanceJobStatsMsg : public SimpleMsg
{
   public:
      GetChunkBalanceJobStatsMsg() : SimpleMsg(NETMSGTYPE_GetChunkBalanceJobStats) { }
};

