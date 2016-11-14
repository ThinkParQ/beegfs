#ifndef SIMPLESTRINGMSG_H_
#define SIMPLESTRINGMSG_H_

#include "NetMessage.h"

class SimpleStringMsg : public NetMessage
{
   protected:
      /**
       * @param value just a reference
       */
      SimpleStringMsg(unsigned short msgType, const char* value) : NetMessage(msgType)
      {
         this->value = value;
         this->valueLen = strlen(value);
      }

      /**
       * @param value just a reference
       */
      SimpleStringMsg(unsigned short msgType, std::string& value) : NetMessage(msgType)
      {
         this->value = value.c_str();
         this->valueLen = value.length();
      }

      /**
       * For deserialization only!
       */
      SimpleStringMsg(unsigned short msgType) : NetMessage(msgType)
      {
      }

      virtual void serializePayload(char* buf)
      {
         Serialization::serializeStr(buf, valueLen, value);
      }

      virtual bool deserializePayload(const char* buf, size_t bufLen)
      {
         unsigned valBufLen;

         return Serialization::deserializeStr(buf, bufLen, &valueLen, &value, &valBufLen);
      }
      
      virtual unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenStr(valueLen); // value
      }
      
      
   private:
      const char* value;
      unsigned valueLen;
      
      
   public:
      // getters & setters
      const char* getValue()
      {
         return value;
      }
   
};

#endif /* SIMPLESTRINGMSG_H_ */
