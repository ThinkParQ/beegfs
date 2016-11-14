#ifndef REFRESHCAPACITYPOOLSMSG_H_
#define REFRESHCAPACITYPOOLSMSG_H_

#include <common/net/message/AcknowledgeableMsg.h>
#include <common/Common.h>


/**
 * This msg notifies the receiver about a change in the capacity pool lists. It is typically sent by
 * the mgmtd to a mds. The receiver should update its capacity pools.
 */
class RefreshCapacityPoolsMsg : public AcknowledgeableMsg
{
   public:
      /**
       * Default constructor for serialization and deserialization.
       */
      RefreshCapacityPoolsMsg() : AcknowledgeableMsg(NETMSGTYPE_RefreshCapacityPools)
      {
         this->ackID = "";
         this->ackIDLen = 0;
      }


   protected:
      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      virtual unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenStr(ackIDLen);
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
      unsigned ackIDLen;
      const char* ackID;


   public:
      // getters & setters
      const char* getAckID()
      {
         return ackID;
      }
};


#endif /* REFRESHCAPACITYPOOLSMSG_H_ */
