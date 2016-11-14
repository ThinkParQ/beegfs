#ifndef REFRESHSESSIONMSG_H_
#define REFRESHSESSIONMSG_H_

#include <common/net/message/NetMessage.h>


class RefreshSessionMsg : public NetMessage
{
   public:
      
      /**
       * @param sessionID just a reference, so do not free it as long as you use this object!
       */
      RefreshSessionMsg(const char* sessionID) :
         NetMessage(NETMSGTYPE_RefreshSession)
      {
         this->sessionID = sessionID;
         this->sessionIDLen = strlen(sessionID);
      }

     
   protected:
      RefreshSessionMsg() : NetMessage(NETMSGTYPE_RefreshSession)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenStr(sessionIDLen); // sessionID
      }


   private:
      unsigned sessionIDLen;
      const char* sessionID;

   public:
   
      // getters & setters
      const char* getSessionID() const
      {
         return sessionID;
      }

};

#endif /* REFRESHSESSIONMSG_H_ */
