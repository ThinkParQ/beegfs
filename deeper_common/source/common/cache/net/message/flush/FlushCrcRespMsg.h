#ifndef NET_MESSAGE_FLUSHCRCRESPMSG_H_
#define NET_MESSAGE_FLUSHCRCRESPMSG_H_


#include <common/net/message/NetMessageTypes.h>
#include <common/net/message/SimpleIntMsg.h>



class FlushCrcRespMsg : public SimpleIntMsg
{
   public:
      FlushCrcRespMsg(int error) : SimpleIntMsg(NETMSGTYPE_CacheFlushCrcResp, error) {};

      /*
       * deserialization only
       */
      FlushCrcRespMsg() : SimpleIntMsg(NETMSGTYPE_CacheFlushCrcResp) {};

      virtual ~FlushCrcRespMsg() {};
};

#endif /* NET_MESSAGE_FLUSHCRCRESPMSG_H_ */
