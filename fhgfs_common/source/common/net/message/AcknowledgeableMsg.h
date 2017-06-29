#ifndef ACKNOWLEDGEABLEMSG_H_
#define ACKNOWLEDGEABLEMSG_H_

#include "NetMessage.h"
#include <common/net/message/control/AckMsg.h>

// Ack messages are used for request that might not be answered immediately. For example
// UDP tranfers or file locks.

class AcknowledgeableMsg : public NetMessage
{
   protected:
      AcknowledgeableMsg(unsigned short msgType, const char* ackID)
         : NetMessage(msgType), ackID(ackID), ackIDLen(strlen(ackID) )
      {
      }

      void serializeAckID(Serializer& ser, unsigned align = 1) const
      {
         ser % serdes::rawString(ackID, ackIDLen, align);
      }

      void serializeAckID(Deserializer& des, unsigned align = 1)
      {
         des % serdes::rawString(ackID, ackIDLen, align);
      }

   private:
      const char* ackID;
      unsigned ackIDLen;

   public:
      // setters & getters

      const char* getAckID() const { return ackID; }
      bool wantsAck() const { return ackIDLen != 0; }

      /**
       * @param ackID just a reference
       */
      void setAckID(const char* ackID)
      {
         this->ackID = ackID;
         this->ackIDLen = strlen(ackID);
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         obj->serializeAckID(ctx);
      }

      bool acknowledge(ResponseContext& ctx)
      {
         if(ackIDLen == 0)
            return false;

         ctx.sendResponse(AckMsg(ackID) );

         return true;
      }
};

template<typename Derived>
class AcknowledgeableMsgSerdes : public AcknowledgeableMsg
{
   protected:
      typedef AcknowledgeableMsgSerdes BaseType;

      AcknowledgeableMsgSerdes(unsigned short msgType, const char* ackID = "")
         : AcknowledgeableMsg(msgType, ackID)
      {
      }

   protected:
      void serializePayload(Serializer& ser) const
      {
         ser % *(Derived*) this;
      }

      bool deserializePayload(const char* buf, size_t bufLen)
      {
         Deserializer des(buf, bufLen);
         des % *(Derived*) this;
         return des.good();
      }
};

#endif /* ACKNOWLEDGEABLEMSG_H_ */
