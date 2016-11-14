#ifndef REQUESTMETADATARESPMSG_H_
#define REQUESTMETADATARESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/Node.h>


class RequestMetaDataRespMsg : public NetMessage
{
private:
   const char* nodeID;
   unsigned nodeIDLen;
   uint16_t nodeNumID;
   bool isRoot;
   unsigned indirectWorkListSize;
   unsigned directWorkListSize; 
   unsigned sessionCount;
   NicAddressList* nicList;

   // for deserialization
   unsigned nicListElemNum; // NETMSG_NICLISTELEM_SIZE defines the element size
   const char* nicListStart; // see NETMSG_NICLISTELEM_SIZE for element structure

   // for serialization
   HighResStatsList* statsList; // not owned by this object!

   // for deserialization
   unsigned statsListElemNum; // NETMSG_NICLISTELEM_SIZE defines the element size
   const char* statsListStart; // see NETMSG_NICLISTELEM_SIZE for element structure
      
   public:
      /**
       * @param nicList just a reference, so do not free it as long as you use this object!
       * @param statsList just a reference, so do not free it as long as you use this object!
       */
      RequestMetaDataRespMsg(const char* nodeID, uint16_t nodeNumID, NicAddressList *nicList,
         bool isRoot, unsigned IndirectWorkListSize, unsigned DirectWorkListSize,
         unsigned sessionCount, HighResStatsList* statsList)
         : NetMessage(NETMSGTYPE_RequestMetaDataResp)
      {
         this->nodeID = nodeID;
         this->nodeIDLen = strlen(this->nodeID);
         this->nodeNumID = nodeNumID;
         this->nicList = nicList;
         this->isRoot = isRoot;
         this->indirectWorkListSize = IndirectWorkListSize;
         this->directWorkListSize = DirectWorkListSize;
         this->sessionCount = sessionCount;
         this->statsList = statsList;
      }

      RequestMetaDataRespMsg() : NetMessage(NETMSGTYPE_RequestMetaDataResp)
      {
         this->nodeID = "";
         this->nodeIDLen = strlen(this->nodeID);
         this->nodeNumID = 0;
         this->nicList = NULL;
         this->isRoot = 0;
         this->indirectWorkListSize = 0;
         this->directWorkListSize = 0;
         this->sessionCount = 0;
      }

      void parseNicList(NicAddressList* outNicList)
      {
         Serialization::deserializeNicList(nicListElemNum, nicListStart, outNicList);
      }

      void parseStatsList(HighResStatsList* outList)
      {
         Serialization::deserializeHighResStatsList(statsListElemNum, statsListStart, outList);
      }

      const char* getNodeID()
      {
         return nodeID;
      }

      uint16_t getNodeNumID()
      {
         return nodeNumID;
      }

      NicAddressList *getNicList()
      {
         return this->nicList;
      }

      bool getIsRoot()
      {
         return isRoot;
      }

      unsigned getIndirectWorkListSize()
      {
         return indirectWorkListSize;
      }

      unsigned getDirectWorkListSize()
      {
         return directWorkListSize;
      }

      unsigned getSessionCount()
      {
         return sessionCount;
      }

   protected:
      virtual void serializePayload(char* buf);
      virtual bool deserializePayload(const char* buf, size_t bufLen);

      unsigned calcMessageLength()
      {
         return NETMSG_HEADER_LENGTH
            + Serialization::serialLenStr(nodeIDLen)                 // nodeID
            + Serialization::serialLenUShort()                       // nodeNUmID
            + Serialization::serialLenNicList(nicList)               // nicList
            + Serialization::serialLenBool()                         // isRoot
            + Serialization::serialLenUInt()                         // IndirectWorkListSize
            + Serialization::serialLenUInt()                         // DirectWorkListSize
            + Serialization::serialLenUInt()                         // sessionCount
            + Serialization::serialLenHighResStatsList(statsList);   // statsList
      }
};

#endif /*REQUESTMETADATARESPMSG_H_*/
