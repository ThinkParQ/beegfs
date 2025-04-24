#pragma once

#include <common/Common.h>
#include <common/net/message/SimpleIntMsg.h>


class UnmapTargetRespMsg : public SimpleIntMsg
{
   public:
      /**
       * @param result FhgfsOpsErr
       */
      UnmapTargetRespMsg(int result) :
         SimpleIntMsg(NETMSGTYPE_UnmapTargetResp, result)
      {
      }

      UnmapTargetRespMsg() :
         SimpleIntMsg(NETMSGTYPE_UnmapTargetResp)
      {
      }
};


