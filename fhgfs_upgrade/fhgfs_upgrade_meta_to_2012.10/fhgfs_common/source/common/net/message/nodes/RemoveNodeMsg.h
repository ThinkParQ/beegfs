#ifndef REMOVENODEMSG_H_
#define REMOVENODEMSG_H_

#include <common/Common.h>
#include <common/net/message/AcknowledgeableMsg.h>

class RemoveNodeMsg : public AcknowledgeableMsg
{
   public:
      
      /**
       * @param nodeID just a reference, so do not free it as long as you use this object!
       * @param nodeType NODETYPE_...
       */
      RemoveNodeMsg(const char* nodeID, uint16_t nodeNumID, NodeType nodeType) :
         AcknowledgeableMsg(NETMSGTYPE_RemoveNode)
      {
         this->nodeID = nodeID;
         this->nodeIDLen = strlen(nodeID);
         
         this->nodeNumID = nodeNumID;

         this->nodeType = (int16_t)nodeType;

         this->ackID = "";
         this->ackIDLen = 0;
      }


   protected:
      RemoveNodeMsg() : AcknowledgeableMsg(NETMSGTYPE_RemoveNode)
      {
      }


      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);
      
      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenShort() + // nodeType
            Serialization::serialLenUShort() + // nodeNumID
            Serialization::serialLenStr(nodeIDLen) +
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
      unsigned nodeIDLen;
      const char* nodeID;
      uint16_t nodeNumID;
      int16_t nodeType; // NODETYPE_...
      unsigned ackIDLen;
      const char* ackID;
      

   public:
   
      // getters & setters
      const char* getNodeID() const
      {
         return nodeID;
      }
      
      uint16_t getNodeNumID() const
      {
         return nodeNumID;
      }

      NodeType getNodeType() const
      {
         return (NodeType)nodeType;
      }

      const char* getAckID()
      {
         return ackID;
      }
      
};


#endif /* REMOVENODEMSG_H_ */
