#ifndef STORAGE_SETDEFAULTQUOTAMSGEX_H_
#define STORAGE_SETDEFAULTQUOTAMSGEX_H_

#include <common/net/message/storage/quota/SetDefaultQuotaMsg.h>

class SetDefaultQuotaMsgEx : public SetDefaultQuotaMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* STORAGE_SETDEFAULTQUOTAMSGEX_H_ */
