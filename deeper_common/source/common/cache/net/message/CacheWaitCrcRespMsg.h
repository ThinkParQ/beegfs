#ifndef COMMON_NET_MESSAGE_CACHEWAITCRCRESPMSG_H_
#define COMMON_NET_MESSAGE_CACHEWAITCRCRESPMSG_H_


#include <common/net/message/NetMessage.h>


class CacheWaitCrcRespMsg : public NetMessageSerdes<CacheWaitCrcRespMsg>
{
   public:
      /*
       * superclass, not for direct use
       */
      CacheWaitCrcRespMsg(unsigned short msgType, int error, unsigned long checksum)
         : BaseType(msgType)
      {
         this->error = error;
         this->checksum = checksum;
      }

      /*
       * superclass, not for direct use, deserialization only
       */
      CacheWaitCrcRespMsg(unsigned short msgType) : BaseType(msgType) {};

      virtual ~CacheWaitCrcRespMsg() {};


   protected:
      int error;
      uint64_t checksum;


   public:
      int getError()
      {
         return this->error;
      }

      unsigned long getChecksum()
      {
         return this->checksum;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->checksum
            % obj->error;
      }
};

#endif /* COMMON_NET_MESSAGE_CACHEWAITCRCRESPMSG_H_ */
