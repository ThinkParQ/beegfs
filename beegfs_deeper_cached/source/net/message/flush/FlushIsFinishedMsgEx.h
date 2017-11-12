#ifndef NET_MESSAGE_FLUSH_FLUSHISFINISHEDMSGEX_H_
#define NET_MESSAGE_FLUSH_FLUSHISFINISHEDMSGEX_H_


#include <common/cache/net/message/flush/FlushIsFinishedMsg.h>



class FlushIsFinishedMsgEx: public FlushIsFinishedMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* NET_MESSAGE_FLUSH_FLUSHISFINISHEDMSGEX_H_ */
