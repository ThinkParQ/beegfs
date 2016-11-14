#ifndef GETHOSTBYNAMERESPMSG_H_
#define GETHOSTBYNAMERESPMSG_H_

#include <common/Common.h>
#include <common/net/message/NetMessage.h>

class GetHostByNameRespMsg : public NetMessage
{
   public:
      
      /**
       * @param hostAddr just a reference, so do not free it as long as you use this object!
       */
      GetHostByNameRespMsg(const char* hostAddr) : NetMessage(NETMSGTYPE_GetHostByNameResp)
      {
         this->hostAddr = hostAddr;
         this->hostAddrLen = strlen(hostAddr);
      }

      GetHostByNameRespMsg() : NetMessage(NETMSGTYPE_GetHostByNameResp)
      {
      }

      
   protected:

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenStr(hostAddrLen);
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
