#ifndef NET_MESSAGE_FLUSHSTOPRESPMSG_H_
#define NET_MESSAGE_FLUSHSTOPRESPMSG_H_


#include <common/net/message/NetMessageTypes.h>
#include <common/net/message/SimpleIntMsg.h>



class FlushStopRespMsg : public SimpleIntMsg
{
   public:
      FlushStopRespMsg(int error) : SimpleIntMsg(NETMSGTYPE_CacheFlushStopResp, error) {};

      /*
       * deserialization only
       */
      FlushStopRespMsg() : SimpleIntMsg(NETMSGTYPE_CacheFlushStopResp) {};

      virtual ~FlushStopRespMsg() {};
};

#endif /* NET_MESSAGE_FLUSHSTOPRESPMSG_H_ */
