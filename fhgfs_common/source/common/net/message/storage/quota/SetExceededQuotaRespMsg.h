#ifndef SETEXCEEDEDQUOTARESPMSG_H_
#define SETEXCEEDEDQUOTARESPMSG_H_


#include <common/net/message/SimpleIntMsg.h>
#include <common/Common.h>


class SetExceededQuotaRespMsg: public SimpleIntMsg
{
   public:
      SetExceededQuotaRespMsg(int result) : SimpleIntMsg(NETMSGTYPE_SetExceededQuotaResp, result)
      {
      }

      /**
       * For deserialization only
       */
      SetExceededQuotaRespMsg() : SimpleIntMsg(NETMSGTYPE_SetExceededQuotaResp)
      {
      }
};

#endif /* SETEXCEEDEDQUOTARESPMSG_H_ */
