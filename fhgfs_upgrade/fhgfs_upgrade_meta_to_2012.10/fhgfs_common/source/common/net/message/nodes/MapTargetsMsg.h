#ifndef MAPTARGETSMSG_H_
#define MAPTARGETSMSG_H_

#include <common/net/message/AcknowledgeableMsg.h>
#include <common/Common.h>


class MapTargetsMsg : public AcknowledgeableMsg
{
   public:
      /**
       * Maps all targetIDs to the same given nodeID.
       *
       * @param targetIDs just a reference => do not free while you're using this object
       * @param nodeID just a reference => do not free while you're using this object
       */
      MapTargetsMsg(UInt16List* targetIDs, uint16_t nodeID) :
         AcknowledgeableMsg(NETMSGTYPE_MapTargets)
      {
         this->targetIDs = targetIDs;

         this->nodeID = nodeID;

         this->ackID = "";
         this->ackIDLen = 0;
      }

      MapTargetsMsg() : AcknowledgeableMsg(NETMSGTYPE_MapTargets)
      {
      }


   protected:

      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      virtual unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenUInt16List(targetIDs) +
            Serialization::serialLenUShort() + // nodeID
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
      uint16_t nodeID;
      unsigned ackIDLen;
      const char* ackID;

      // for serialization
      UInt16List* targetIDs; // not owned by this object!

      // for deserialization
      unsigned targetIDsElemNum;
      const char* targetIDsListStart;
      unsigned targetIDsBufLen;


   public:
      // inliners
      void parseTargetIDs(UInt16List* outTargetIDs)
      {
         Serialization::deserializeUInt16List(
            targetIDsBufLen, targetIDsElemNum, targetIDsListStart, outTargetIDs);
      }

      // getters & setters
      uint16_t getNodeID() const
      {
         return nodeID;
      }

      const char* getAckID()
      {
         return ackID;
      }
};

#endif /* MAPTARGETSMSG_H_ */
