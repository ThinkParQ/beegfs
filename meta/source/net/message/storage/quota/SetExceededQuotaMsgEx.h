#ifndef META_SETEXCEEDEDQUOTAMSGEX_H_
#define META_SETEXCEEDEDQUOTAMSGEX_H_


#include <common/net/message/storage/quota/SetExceededQuotaMsg.h>
#include <common/Common.h>


class SetExceededQuotaMsgEx : public SetExceededQuotaMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* META_SETEXCEEDEDQUOTAMSGEX_H_ */
