#ifndef NET_MESSAGE_FLUSH_FLUSHWAITCRCMSGEX_H_
#define NET_MESSAGE_FLUSH_FLUSHWAITCRCMSGEX_H_


#include <common/cache/net/message/flush/FlushWaitCrcMsg.h>



class FlushWaitCrcMsgEx: public FlushWaitCrcMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);
};

#endif /* NET_MESSAGE_FLUSH_FLUSHWAITCRCMSGEX_H_ */
