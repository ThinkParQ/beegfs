#ifndef SIMPLEINTMSG_H_
#define SIMPLEINTMSG_H_

#include "NetMessage.h"

class SimpleIntMsg : public NetMessageSerdes<SimpleIntMsg>
{
   protected:
      SimpleIntMsg(unsigned short msgType, int value) : BaseType(msgType)
      {
         this->value = value;
      }

      SimpleIntMsg(unsigned short msgType) : BaseType(msgType)
      {
      }

   public:
      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->value;
      }

   private:
      int32_t value;

   public:
      int getValue() const { return value; }
};

#endif /*SIMPLEINTMSG_H_*/
