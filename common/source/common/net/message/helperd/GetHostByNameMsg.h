#ifndef GETHOSTBYNAMEMSG_H_
#define GETHOSTBYNAMEMSG_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>

class GetHostByNameMsg : public NetMessageSerdes<GetHostByNameMsg>
{
   public:

      /**
       * @param hostname just a reference, so do not free it as long as you use this object!
       */
      GetHostByNameMsg(const char* hostname) :
         BaseType(NETMSGTYPE_GetHostByName)
      {
         this->hostname = hostname;
         this->hostnameLen = strlen(hostname);
      }

      GetHostByNameMsg() : BaseType(NETMSGTYPE_GetHostByName)
      {
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::rawString(obj->hostname, obj->hostnameLen);
      }

   private:
      unsigned hostnameLen;
      const char* hostname;


   public:

      // getters & setters
      const char* getHostname()
      {
         return hostname;
      }
};

#endif /*GETHOSTBYNAMEMSG_H_*/
