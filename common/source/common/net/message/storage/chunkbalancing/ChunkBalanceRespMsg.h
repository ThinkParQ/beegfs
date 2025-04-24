#pragma once

#include <common/net/message/SimpleIntMsg.h>

class ChunkBalanceRespMsg : public SimpleIntMsg
{
   public:
      ChunkBalanceRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_ChunkBalanceResp, result) {}

      ChunkBalanceRespMsg() : SimpleIntMsg(NETMSGTYPE_ChunkBalanceResp) {}
};


