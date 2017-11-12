#ifndef NET_MESSAGE_FLUSH_PREFETCHMSGEX_H_
#define NET_MESSAGE_FLUSH_PREFETCHMSGEX_H_


#include <common/cache/net/message/prefetch/PrefetchMsg.h>



class PrefetchMsgEx: public PrefetchMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* NET_MESSAGE_FLUSH_PREFETCHMSGEX_H_ */
