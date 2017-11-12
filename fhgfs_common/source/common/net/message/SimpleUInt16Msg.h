#ifndef SIMPLEUINT16MSG_H_
#define SIMPLEUINT16MSG_H_

#include "NetMessage.h"

class SimpleUInt16Msg : public NetMessageSerdes<SimpleUInt16Msg>
{
   protected:
      SimpleUInt16Msg(unsigned short msgType, uint16_t value) : BaseType(msgType)
      {
         this->value = value;
      }

      /**
       * Constructor for deserialization only!
       */
      SimpleUInt16Msg(unsigned short msgType) : BaseType(msgType)
      {
      }

   public:
      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->value;
      }

   private:
      uint16_t value;

   public:
      uint16_t getValue() const { return value; }
};

#endif /* SIMPLEUINT16MSG_H_ */
