#pragma once

#include <common/net/message/SimpleIntMsg.h>

class StripePatternUpdateRespMsg : public SimpleIntMsg
{
   public:
      StripePatternUpdateRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_StripePatternUpdateResp, result) {}

     StripePatternUpdateRespMsg() : SimpleIntMsg(NETMSGTYPE_StripePatternUpdateResp) {}

   private:


   public:
      // inliners
      FhgfsOpsErr getResult()
      {
         return static_cast<FhgfsOpsErr>(getValue());
      }
};



