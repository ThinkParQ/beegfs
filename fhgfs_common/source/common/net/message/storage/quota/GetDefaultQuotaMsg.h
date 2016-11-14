#ifndef COMMON_NET_MESSAGE_STORAGE_QUOTA_GETDEFAULTQUOTAMSG_H_
#define COMMON_NET_MESSAGE_STORAGE_QUOTA_GETDEFAULTQUOTAMSG_H_


#include <common/net/message/SimpleMsg.h>

class GetDefaultQuotaMsg : public SimpleMsg
{
   public:
      GetDefaultQuotaMsg() : SimpleMsg(NETMSGTYPE_GetDefaultQuota) {};
      virtual ~GetDefaultQuotaMsg() {};
};

#endif /* COMMON_NET_MESSAGE_STORAGE_QUOTA_GETDEFAULTQUOTAMSG_H_ */
