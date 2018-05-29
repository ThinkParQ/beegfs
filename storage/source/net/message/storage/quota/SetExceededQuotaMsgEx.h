#ifndef SETEXCEEDEDQUOTAMSGEX_H_
#define SETEXCEEDEDQUOTAMSGEX_H_


#include <common/net/message/storage/quota/SetExceededQuotaMsg.h>
#include <common/Common.h>


class SetExceededQuotaMsgEx : public SetExceededQuotaMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* SETEXCEEDEDQUOTAMSGEX_H_ */
