#ifndef SIMPLEUINT16MSG_H_
#define SIMPLEUINT16MSG_H_

#include "NetMessage.h"

class SimpleUInt16Msg : public NetMessage
{
   protected:
      SimpleUInt16Msg(unsigned short msgType, uint16_t value) : NetMessage(msgType)
      {
         this->value = value;
      }

      /**
       * Constructor for deserialization only!
       */
      SimpleUInt16Msg(unsigned short msgType) : NetMessage(msgType)
      {
      }


      virtual void serializePayload(char* buf)
      {
         Serialization::serializeUShort(buf, value);
      }

      virtual bool deserializePayload(const char* buf, size_t bufLen)
      {
         unsigned valLen;

         return Serialization::deserializeUShort(buf, bufLen, &value, &valLen);
      }

      virtual unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenUShort(); // value
      }

   private:
      uint16_t value;

   public:
      // getters & setters
      uint16_t getValue() const
      {
         return value;
      }


};

#endif /* SIMPLEUINT16MSG_H_ */
