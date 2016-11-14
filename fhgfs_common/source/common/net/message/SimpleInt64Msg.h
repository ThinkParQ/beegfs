#ifndef SIMPLEINT64MSG_H_
#define SIMPLEINT64MSG_H_

#include "NetMessage.h"

class SimpleInt64Msg : public NetMessageSerdes<SimpleInt64Msg>
{
   protected:
      SimpleInt64Msg(unsigned short msgType, int64_t value) : BaseType(msgType)
      {
         this->value = value;
      }

      SimpleInt64Msg(unsigned short msgType) : BaseType(msgType)
      {
      }

   public:
      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->value;
      }

   private:
      int64_t value;

   public:
      int64_t getValue() const { return value; }
};

#endif /*SIMPLEINT64MSG_H_*/
