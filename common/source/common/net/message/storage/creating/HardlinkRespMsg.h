#pragma once

#include <common/net/message/SimpleIntMsg.h>

class HardlinkRespMsg : public SimpleIntMsg
{
   public:
      HardlinkRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_HardlinkResp, result)
      {
      }

      HardlinkRespMsg() : SimpleIntMsg(NETMSGTYPE_HardlinkResp)
      {
      }

      // getters & setters
      FhgfsOpsErr getResult()
      {
         return (FhgfsOpsErr)getValue();
      }
};


