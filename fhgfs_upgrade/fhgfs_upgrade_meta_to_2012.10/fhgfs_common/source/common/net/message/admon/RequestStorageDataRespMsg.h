#ifndef REQUESTSTORAGEDATARESPMSG_H_
#define REQUESTSTORAGEDATARESPMSG_H_

#include <common/net/message/NetMessage.h>
#include <common/nodes/Node.h>
#include <common/components/worker/MultiWorkQueue.h>
#include <common/toolkit/HighResolutionStats.h>


class RequestStorageDataRespMsg: public NetMessage
{
   private:
      const char* nodeID;
      unsigned nodeIDLen;
      uint16_t nodeNumID;
      NicAddressList* nicList;
      unsigned indirectWorkListSize;
      unsigned directWorkListSize;
      int64_t diskSpaceTotalMB;
      int64_t diskSpaceFreeMB;
      unsigned sessionCount;
      // for deserialization
      unsigned nicListElemNum; // NETMSG_NICLISTELEM_SIZE defines the element size
      const char* nicListStart; // see NETMSG_NICLISTELEM_SIZE for element structure
      unsigned statsListElemNum; // NETMSG_NICLISTELEM_SIZE defines the element size
      const char* statsListStart; // see NETMSG_NICLISTELEM_SIZE for element structure
      unsigned storageTargetsListElemNum;
      const char* storageTargetsListStart;
      // for serialization
      HighResStatsList* statsList; // not owned by this object!
      StorageTargetInfoList *storageTargets; // not owned by this object!

   public:
      /**
       * @param nicList just a reference, so do not free it as long as you use this object!
       * @param statsList just a reference, so do not free it as long as you use this object!
       */
      RequestStorageDataRespMsg(const char* nodeID, uint16_t nodeNumID, NicAddressList *nicList,
         unsigned indirectWorkListSize, unsigned directWorkListSize, int64_t diskSpaceTotal,
         int64_t diskSpaceFree, unsigned sessionCount, HighResStatsList* statsList,
         StorageTargetInfoList *storageTargets) :
         NetMessage(NETMSGTYPE_RequestStorageDataResp)
      {
         this->nodeID = nodeID;
         this->nodeIDLen = strlen(this->nodeID);
         this->nodeNumID = nodeNumID;
         this->nicList = nicList;
         this->indirectWorkListSize = indirectWorkListSize;
         this->directWorkListSize = directWorkListSize;
         this->diskSpaceTotalMB = diskSpaceTotal / 1024 / 1024;
         this->diskSpaceFreeMB = diskSpaceFree / 1024 / 1024;
         this->sessionCount = sessionCount;
         this->statsList = statsList;
         this->storageTargets = storageTargets;
      }
      ;

      RequestStorageDataRespMsg() :
         NetMessage(NETMSGTYPE_RequestStorageDataResp)
      {
         this->nodeID = "";
         this->nodeIDLen = strlen(this->nodeID);
         this->nodeNumID = 0;
         this->indirectWorkListSize = 0;
         this->directWorkListSize = 0;
         this->diskSpaceTotalMB = 0;
         this->diskSpaceFreeMB = 0;
         this->sessionCount = 0;
         this->statsList = NULL;
         this->storageTargets = NULL;
      }
      ;

      void parseNicList(NicAddressList* outNicList)
      {
         Serialization::deserializeNicList(nicListElemNum, nicListStart, outNicList);
      }

      void parseStatsList(HighResStatsList* outList)
      {
         Serialization::deserializeHighResStatsList(statsListElemNum, statsListStart, outList);
      }

      void parseStorageTargets(StorageTargetInfoList *outList)
      {
         Serialization::deserializeStorageTargetInfoList(storageTargetsListElemNum,
            storageTargetsListStart, outList);
      }

      const char* getNodeID()
      {
         return nodeID;
      }

      uint16_t getNodeNumID()
      {
         return nodeNumID;
      }

      unsigned getIndirectWorkListSize()
      {
         return indirectWorkListSize;
      }

      unsigned getDirectWorkListSize()
      {
         return directWorkListSize;
      }

      int64_t getDiskSpaceTotalMB()
      {
         return diskSpaceTotalMB;
      }

      int64_t getDiskSpaceFreeMB()
      {
         return diskSpaceFreeMB;
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
         return NETMSG_HEADER_LENGTH +
            Serialization::serialLenStr(nodeIDLen) +                       // nodeID
            Serialization::serialLenUShort() +                             // nodeNumID
            Serialization::serialLenNicList(nicList) +                     // nicList
            Serialization::serialLenUInt() +                               // indirectWorkQueueSize
            Serialization::serialLenUInt() +                               // directWorkQueueSize
            Serialization::serialLenInt64() +                              // diskSpaceTotal
            Serialization::serialLenInt64() +                              // diskSpaceFree
            Serialization::serialLenUInt() +                               // sessionCount
            Serialization::serialLenHighResStatsList(statsList) +          // statsList
            Serialization::serialLenStorageTargetInfoList(storageTargets); // storageTargets
      }

};

#endif /*REQUESTSTORAGEDATARESPMSG_H_*/
