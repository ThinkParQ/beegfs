#ifndef GETHOSTBYNAMERESPMSG_H_
#define GETHOSTBYNAMERESPMSG_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>

class GetHostByNameRespMsg : public NetMessageSerdes<GetHostByNameRespMsg>
{
   public:

      /**
       * @param hostAddr just a reference, so do not free it as long as you use this object!
       */
      GetHostByNameRespMsg(const char* hostAddr) : BaseType(NETMSGTYPE_GetHostByNameResp)
      {
         this->hostAddr = hostAddr;
         this->hostAddrLen = strlen(hostAddr);
      }

      GetHostByNameRespMsg() : BaseType(NETMSGTYPE_GetHostByNameResp)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::rawString(obj->hostAddr, obj->hostAddrLen);
      }

   private:
      unsigned hostAddrLen;
      const char* hostAddr;


   public:

      // getters & setters
      const char* getHostAddr()
      {
         return hostAddr;
      }
};

#endif /*GETHOSTBYNAMERESPMSG_H_*/
