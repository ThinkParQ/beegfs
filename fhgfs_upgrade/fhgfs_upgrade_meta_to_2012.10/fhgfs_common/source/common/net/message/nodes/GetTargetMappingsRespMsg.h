#ifndef GETTARGETMAPPINGSRESPMSG_H_
#define GETTARGETMAPPINGSRESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/Common.h>


class GetTargetMappingsRespMsg : public NetMessage
{
   public:
      /**
       * @param targetIDs just a reference => do not free while you're using this object
       * @param nodeIDs just a reference => do not free while you're using this object
       */
      GetTargetMappingsRespMsg(UInt16List* targetIDs, UInt16List* nodeIDs) :
         NetMessage(NETMSGTYPE_GetTargetMappingsResp)
      {
         this->targetIDs = targetIDs;
         this->nodeIDs = nodeIDs;
      }

      GetTargetMappingsRespMsg() : NetMessage(NETMSGTYPE_GetTargetMappingsResp)
      {
      }


   protected:
      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      virtual unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenUInt16List(targetIDs) +
            Serialization::serialLenUInt16List(nodeIDs);
      }


   private:
      // for serialization
      UInt16List* targetIDs; // not owned by this object!
      UInt16List* nodeIDs; // not owned by this object!

      // for deserialization
      unsigned targetIDsElemNum;
      const char* targetIDsListStart;
      unsigned targetIDsBufLen;
      unsigned nodeIDsElemNum;
      const char* nodeIDsListStart;
      unsigned nodeIDsBufLen;


   public:
      // inliners
      void parseTargetIDs(UInt16List* outTargetIDs)
      {
         Serialization::deserializeUInt16List(
            targetIDsBufLen, targetIDsElemNum, targetIDsListStart, outTargetIDs);
      }

      void parseNodeIDs(UInt16List* outNodeIDs)
      {
         Serialization::deserializeUInt16List(
            nodeIDsBufLen, nodeIDsElemNum, nodeIDsListStart, outNodeIDs);
      }

};

#endif /* GETTARGETMAPPINGSRESPMSG_H_ */
