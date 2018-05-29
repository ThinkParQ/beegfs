#ifndef NET_MESSAGE_FLUSHWAITRESPMSG_H_
#define NET_MESSAGE_FLUSHWAITRESPMSG_H_


#include <common/net/message/NetMessageTypes.h>
#include <common/net/message/SimpleIntMsg.h>



class FlushWaitRespMsg : public SimpleIntMsg
{
   public:
      FlushWaitRespMsg(int error) : SimpleIntMsg(NETMSGTYPE_CacheFlushWaitResp, error) {};

      /*
       * deserialization only
       */
      FlushWaitRespMsg() : SimpleIntMsg(NETMSGTYPE_CacheFlushWaitResp) {};

      virtual ~FlushWaitRespMsg() {};
};

#endif /* NET_MESSAGE_FLUSHWAITRESPMSG_H_ */
