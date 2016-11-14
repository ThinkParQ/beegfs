#ifndef ACQUIREAPPENDLOCKMSG_H_
#define ACQUIREAPPENDLOCKMSG_H_

#include <common/net/message/NetMessage.h>

class AcquireAppendLockMsg : public NetMessage
{
   public:
      
      /**
       * @param sessionID just a reference, so do not free it as long as you use this object!
       */
      AcquireAppendLockMsg(const char* sessionID, int fd) :
         NetMessage(NETMSGTYPE_AcquireAppendLock)
      {
         this->sessionID = sessionID;
         this->sessionIDLen = strlen(sessionID);
         
         this->fd = fd;
      }


   protected:
      AcquireAppendLockMsg() : NetMessage(NETMSGTYPE_AcquireAppendLock)
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

#endif /*ACQUIREAPPENDLOCKMSG_H_*/
