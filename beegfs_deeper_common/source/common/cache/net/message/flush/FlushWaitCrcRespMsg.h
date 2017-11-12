#ifndef NET_MESSAGE_FLUSHWAITCRCRESPMSG_H_
#define NET_MESSAGE_FLUSHWAITCRCRESPMSG_H_


#include <common/cache/net/message/CacheWaitCrcRespMsg.h>
#include <common/net/message/NetMessageTypes.h>



class FlushWaitCrcRespMsg : public CacheWaitCrcRespMsg
{
   public:
      FlushWaitCrcRespMsg(int error, unsigned long checksum)
         : CacheWaitCrcRespMsg(NETMSGTYPE_CacheFlushCrcWaitResp, error, checksum) {};

      /*
       * deserialization only
       */
      FlushWaitCrcRespMsg() : CacheWaitCrcRespMsg(NETMSGTYPE_CacheFlushCrcWaitResp) {};

      virtual ~FlushWaitCrcRespMsg() {};
};

#endif /* NET_MESSAGE_FLUSHWAITCRCRESPMSG_H_ */
