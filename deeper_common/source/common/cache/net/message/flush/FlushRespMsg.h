#ifndef NET_MESSAGE_FLUSHRESPMSG_H_
#define NET_MESSAGE_FLUSHRESPMSG_H_


#include <common/net/message/NetMessageTypes.h>
#include <common/net/message/SimpleIntMsg.h>



class FlushRespMsg : public SimpleIntMsg
{
   public:
      FlushRespMsg(int error) : SimpleIntMsg(NETMSGTYPE_CacheFlushResp, error) {};

      /*
       * deserialization only
       */
      FlushRespMsg() : SimpleIntMsg(NETMSGTYPE_CacheFlushResp) {};

      virtual ~FlushRespMsg() {};
};

#endif /* NET_MESSAGE_FLUSHRESPMSG_H_ */
