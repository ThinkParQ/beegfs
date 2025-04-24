#pragma once


#include <common/net/message/SimpleIntMsg.h>



class SetDefaultQuotaRespMsg : public SimpleIntMsg
{
   public:
      SetDefaultQuotaRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_SetDefaultQuotaResp, result) {};

      /**
       * For deserialization only
       */
      SetDefaultQuotaRespMsg() : SimpleIntMsg(NETMSGTYPE_SetDefaultQuotaResp) {};
};

