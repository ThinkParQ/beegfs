#pragma once


#include <common/net/message/SimpleIntMsg.h>


class SetQuotaRespMsg: public SimpleIntMsg
{
   public:
      SetQuotaRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_SetQuotaResp, result)
      {
      }

      /**
       * For deserialization only
       */
      SetQuotaRespMsg() : SimpleIntMsg(NETMSGTYPE_SetQuotaResp)
      {
      }
};

