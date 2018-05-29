#ifndef NET_MESSAGE_PREFETCH_PREFETCHWAITMSGEX_H_
#define NET_MESSAGE_PREFETCH_PREFETCHWAITMSGEX_H_


#include <common/cache/net/message/prefetch/PrefetchWaitMsg.h>



class PrefetchWaitMsgEx: public PrefetchWaitMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* NET_MESSAGE_PREFETCH_PREFETCHWAITMSGEX_H_ */
