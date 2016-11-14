#ifndef LOCKGRANTEDMSG_H_
#define LOCKGRANTEDMSG_H_

#include <common/Common.h>
#include <common/net/message/AcknowledgeableMsg.h>


class LockGrantedMsg : public AcknowledgeableMsg
{
   public:

      /**
       * @param lockAckID just a reference, so do not free it as long as you use this object!
       * @param ackID reply ack; just a reference, so do not free it as long as you use this object!
       * @param granterNodeID nodeID of the sender of this msg (=> receiver of the ack); just a
       * reference, so do not free it as long as you use this object!
       */
      LockGrantedMsg(const std::string& lockAckID, const std::string& ackID,
         uint16_t granterNodeID) :
         AcknowledgeableMsg(NETMSGTYPE_LockGranted)
      {
         this->lockAckID = lockAckID.c_str();
         this->lockAckIDLen = lockAckID.length();

         this->ackID = ackID.c_str();
         this->ackIDLen = ackID.length();

         this->granterNodeID = granterNodeID;
      }


   protected:
      /**
       * Constructor for deserialization only
       */
      LockGrantedMsg() : AcknowledgeableMsg(NETMSGTYPE_LockGranted)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenStrAlign4(lockAckIDLen) +
            Serialization::serialLenStrAlign4(ackIDLen) +
            Serialization::serialLenUShort(); // granterNodeID
      }


      /**
       * @param ackID just a reference
       */
      void setAckIDInternal(const char* ackID)
      {
         this->ackID = ackID;
         this->ackIDLen = strlen(ackID);
      }


   private:
      unsigned lockAckIDLen;
      const char* lockAckID;
      unsigned ackIDLen;
      const char* ackID;
      uint16_t granterNodeID;


   public:

      // getters & setters
      const char* getLockAckID() const
      {
         return lockAckID;
      }

      const char* getAckID()
      {
         return ackID;
      }

      uint16_t getGranterNodeID()
      {
         return granterNodeID;
      }

};


#endif /* LOCKGRANTEDMSG_H_ */
