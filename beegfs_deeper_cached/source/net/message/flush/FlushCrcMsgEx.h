#ifndef NET_MESSAGE_FLUSH_FLUSHCRCMSGEX_H_
#define NET_MESSAGE_FLUSH_FLUSHCRCMSGEX_H_


#include <common/cache/net/message/flush/FlushCrcMsg.h>



class FlushCrcMsgEx: public FlushCrcMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* NET_MESSAGE_FLUSH_FLUSHCRCMSGEX_H_ */
