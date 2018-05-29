#ifndef NET_MESSAGE_PREFETCH_PREFETCHISFINISHEDMSGEX_H_
#define NET_MESSAGE_PREFETCH_PREFETCHISFINISHEDMSGEX_H_


#include <common/cache/net/message/prefetch/PrefetchIsFinishedMsg.h>



class PrefetchIsFinishedMsgEx: public PrefetchIsFinishedMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* NET_MESSAGE_PREFETCH_PREFETCHISFINISHEDMSGEX_H_ */
