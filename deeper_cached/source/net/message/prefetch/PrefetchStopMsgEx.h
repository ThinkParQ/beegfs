#ifndef NET_MESSAGE_PREFETCH_PREFETCHSTOPMSGEX_H_
#define NET_MESSAGE_PREFETCH_PREFETCHSTOPMSGEX_H_


#include <common/cache/net/message/prefetch/PrefetchStopMsg.h>



class PrefetchStopMsgEx: public PrefetchStopMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* NET_MESSAGE_PREFETCH_PREFETCHSTOPMSGEX_H_ */
