#ifndef NET_MESSAGE_PREFETCH_PREFETCHWAITCRCMSGEX_H_
#define NET_MESSAGE_PREFETCH_PREFETCHWAITCRCMSGEX_H_


#include <common/cache/net/message/prefetch/PrefetchWaitCrcMsg.h>



class PrefetchWaitCrcMsgEx: public PrefetchWaitCrcMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* NET_MESSAGE_PREFETCH_PREFETCHWAITCRCMSGEX_H_ */
