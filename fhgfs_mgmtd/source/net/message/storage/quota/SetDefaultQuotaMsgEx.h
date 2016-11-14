#ifndef SOURCE_NET_MESSAGE_STORAGE_QUOTA_SETDEFAULTQUOTAMSGEX_H_
#define SOURCE_NET_MESSAGE_STORAGE_QUOTA_SETDEFAULTQUOTAMSGEX_H_


#include <common/net/message/storage/quota/SetDefaultQuotaMsg.h>



class SetDefaultQuotaMsgEx : public SetDefaultQuotaMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* SOURCE_NET_MESSAGE_STORAGE_QUOTA_SETDEFAULTQUOTAMSGEX_H_ */
