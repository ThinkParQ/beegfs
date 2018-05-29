#ifndef COMMON_NET_MESSAGE_STORAGE_QUOTA_SETDEFAULTQUOTARESPMSG_H_
#define COMMON_NET_MESSAGE_STORAGE_QUOTA_SETDEFAULTQUOTARESPMSG_H_


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

#endif /* COMMON_NET_MESSAGE_STORAGE_QUOTA_SETDEFAULTQUOTARESPMSG_H_ */
