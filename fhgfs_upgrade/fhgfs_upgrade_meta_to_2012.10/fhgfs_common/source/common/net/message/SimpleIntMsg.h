#ifndef SIMPLEINTMSG_H_
#define SIMPLEINTMSG_H_

#include "NetMessage.h"

class SimpleIntMsg : public NetMessage
{
   protected:
      SimpleIntMsg(unsigned short msgType, int value) : NetMessage(msgType)
      {
         this->value = value;
      }

      SimpleIntMsg(unsigned short msgType) : NetMessage(msgType)
      {
      }
      

      virtual void serializePayload(char* buf)
      {
         Serialization::serializeInt(buf, value);
      }

      virtual bool deserializePayload(const char* buf, size_t bufLen)
      {
         unsigned valLen;

         return Serialization::deserializeInt(buf, bufLen, &value, &valLen);
      }
      
      virtual unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenInt(); // value
      }
      
   private:
      int value;
      
   public:
      // getters & setters
      int getValue() const
      {
         return value;
      }
   
   
};

#endif /*SIMPLEINTMSG_H_*/
