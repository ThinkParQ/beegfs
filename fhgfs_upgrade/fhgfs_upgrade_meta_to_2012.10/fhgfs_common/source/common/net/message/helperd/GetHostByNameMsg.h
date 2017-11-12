#ifndef GETHOSTBYNAMEMSG_H_
#define GETHOSTBYNAMEMSG_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>

class GetHostByNameMsg : public NetMessage
{
   public:
      
      /**
       * @param hostname just a reference, so do not free it as long as you use this object!
       */
      GetHostByNameMsg(const char* hostname) :
         NetMessage(NETMSGTYPE_GetHostByName)
      {
         this->hostname = hostname;
         this->hostnameLen = strlen(hostname);
      }


   protected:
      GetHostByNameMsg() : NetMessage(NETMSGTYPE_GetHostByName)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenStr(hostnameLen);
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
