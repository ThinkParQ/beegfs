#pragma once

#include <common/net/message/SimpleIntMsg.h>

class MoveChunkFileRespMsg : public SimpleIntMsg
{
   public:
      MoveChunkFileRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_MoveChunkFileResp, result)
      {
      }

      /**
       * For deserialization only
       */
      MoveChunkFileRespMsg() : SimpleIntMsg(NETMSGTYPE_MoveChunkFileResp)
      {
      }
};


