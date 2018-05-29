#ifndef NET_MESSAGE_FLUSHISFINISHEDRESPMSG_H_
#define NET_MESSAGE_FLUSHISFINISHEDRESPMSG_H_


#include <common/net/message/NetMessageTypes.h>
#include <common/net/message/SimpleIntMsg.h>



class FlushIsFinishedRespMsg : public SimpleIntMsg
{
   public:
      FlushIsFinishedRespMsg(int error) :
         SimpleIntMsg(NETMSGTYPE_CacheFlushIsFinishedResp, error) {};

      /*
       * deserialization only
       */
      FlushIsFinishedRespMsg() : SimpleIntMsg(NETMSGTYPE_CacheFlushIsFinishedResp) {};

      virtual ~FlushIsFinishedRespMsg() {};
};

#endif /* NET_MESSAGE_FLUSHISFINISHEDRESPMSG_H_ */
