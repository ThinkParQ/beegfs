#ifndef ACKNOWLEDGEABLEMSG_H_
#define ACKNOWLEDGEABLEMSG_H_

#include "NetMessage.h"

// Ack messages are used for request that might not be answered immediately. For example
// UDP tranfers or file locks.

class AcknowledgeableMsg : public NetMessage
{
   public:
      virtual ~AcknowledgeableMsg()
      {
      }
   
      
   protected:
      AcknowledgeableMsg(unsigned short msgType) : NetMessage(msgType)
      {
      }

      // setters & getters
      virtual void setAckIDInternal(const char* ackID) = 0;
      
      
   public:
      // setters & getters
      
      virtual const char* getAckID() = 0;
      
      /**
       * @param ackID just a reference
       */
      void setAckID(const char* ackID)
      {
         setAckIDInternal(ackID);
         invalidateMsgLength();
      }
};

#endif /* ACKNOWLEDGEABLEMSG_H_ */
