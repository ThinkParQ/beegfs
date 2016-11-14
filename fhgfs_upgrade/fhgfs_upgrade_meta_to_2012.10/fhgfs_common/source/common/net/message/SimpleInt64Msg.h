#ifndef SIMPLEINT64MSG_H_
#define SIMPLEINT64MSG_H_

#include "NetMessage.h"

class SimpleInt64Msg : public NetMessage
{
   protected:
      SimpleInt64Msg(unsigned short msgType, int64_t value) : NetMessage(msgType)
      {
         this->value = value;
      }

      SimpleInt64Msg(unsigned short msgType) : NetMessage(msgType)
      {
      }

      virtual void serializePayload(char* buf)
      {
         Serialization::serializeInt64(buf, value);
      }

      virtual bool deserializePayload(const char* buf, size_t bufLen)
      {
         unsigned valLen;

         return Serialization::deserializeInt64(buf, bufLen, &value, &valLen);
      }
      
      virtual unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt64(); // value
      }
      
   private:
      int64_t value;
      
   public:
      // getters & setters
      int64_t getValue()
      {
         return value;
      }
   
   
};

#endif /*SIMPLEINT64MSG_H_*/
