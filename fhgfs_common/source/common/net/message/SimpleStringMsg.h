#ifndef SIMPLESTRINGMSG_H_
#define SIMPLESTRINGMSG_H_

#include "NetMessage.h"

class SimpleStringMsg : public NetMessageSerdes<SimpleStringMsg>
{
   protected:
      /**
       * @param value just a reference
       */
      SimpleStringMsg(unsigned short msgType, const char* value) : BaseType(msgType)
      {
         this->value = value;
         this->valueLen = strlen(value);
      }

      /**
       * @param value just a reference
       */
      SimpleStringMsg(unsigned short msgType, const std::string& value) : BaseType(msgType)
      {
         this->value = value.c_str();
         this->valueLen = value.length();
      }

      /**
       * For deserialization only!
       */
      SimpleStringMsg(unsigned short msgType) : BaseType(msgType)
      {
      }

   public:
      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::rawString(obj->value, obj->valueLen);
      }

   private:
      const char* value;
      unsigned valueLen;

   public:
      const char* getValue() const { return value; }
};

#endif /* SIMPLESTRINGMSG_H_ */
