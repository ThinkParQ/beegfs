#ifndef RELEASEAPPENDLOCKMSG_H_
#define RELEASEAPPENDLOCKMSG_H_

#include <common/net/message/NetMessage.h>

class ReleaseAppendLockMsg : public NetMessage
{
   public:
      
      /**
       * @param sessionID just a reference, so do not free it as long as you use this object!
       */
      ReleaseAppendLockMsg(const char* sessionID, int fd) :
         NetMessage(NETMSGTYPE_ReleaseAppendLock)
      {
         this->sessionID = sessionID;
         this->sessionIDLen = strlen(sessionID);
         
         this->fd = fd;
      }


   protected:
      ReleaseAppendLockMsg() : NetMessage(NETMSGTYPE_ReleaseAppendLock)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenStr(sessionIDLen) +
            Serialization::serialLenInt(); // fd
      }


   private:
      unsigned sessionIDLen;
      const char* sessionID;
      int fd;


   public:
      // getters & setters
      const char* getSessionID()
      {
         return sessionID;
      }
      
      int getFD()
      {
         return fd;
      }

};

#endif /*RELEASEAPPENDLOCKMSG_H_*/
