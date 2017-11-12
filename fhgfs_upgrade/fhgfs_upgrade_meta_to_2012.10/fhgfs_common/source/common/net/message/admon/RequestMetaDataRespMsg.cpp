#include "RequestMetaDataRespMsg.h"

bool RequestMetaDataRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // ID
   unsigned nodeBufLen = 0;
   
   if(!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos,
      &nodeIDLen, &nodeID, &nodeBufLen) )
      return false;
   
   bufPos += nodeBufLen;
   
   // nodeNumID
   unsigned nodeNumIDBufLen = 0;

   if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos, &nodeNumID, &nodeNumIDBufLen))
      return false;

   bufPos += nodeNumIDBufLen;

   // NicList
   unsigned nicListBufLen = 0;
   
   if(!Serialization::deserializeNicListPreprocess(&buf[bufPos], bufLen-bufPos,
      &nicListElemNum, &nicListStart, &nicListBufLen) )
      return false;
   
   bufPos += nicListBufLen;
   
   // isRoot
   unsigned isRootBufLen = 0;

   if(!Serialization::deserializeBool(&buf[bufPos], bufLen-bufPos, &isRoot, &isRootBufLen) )
      return false;
   
   bufPos += isRootBufLen;
   
   // IndirectWorkListSize
   unsigned IndirectWorkListSizeBufLen = 0;
   
   if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &indirectWorkListSize,
      &IndirectWorkListSizeBufLen) )
      return false;
   
   bufPos += IndirectWorkListSizeBufLen;
   
   // DirectWorkListSize
   unsigned DirectWorkListSizeBufLen = 0;
   
   if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &directWorkListSize,
      &DirectWorkListSizeBufLen) )
      return false;
   
   bufPos += DirectWorkListSizeBufLen;

   // SessionCount
   unsigned sessionCountBufLen = 0;
   
   if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &sessionCount,
      &sessionCountBufLen) )
      return false;
   
   bufPos += sessionCountBufLen;
   
   // statsList

   unsigned statsListBufLen;

   if(!Serialization::deserializeHighResStatsListPreprocess(&buf[bufPos], bufLen-bufPos,
      &statsListElemNum, &statsListStart, &statsListBufLen) )
      return false;

   bufPos += statsListBufLen;

   return true;
}

void RequestMetaDataRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // NodeId
   bufPos += Serialization::serializeStr(&buf[bufPos], nodeIDLen, nodeID);
   
   // nodeNumID
   bufPos += Serialization::serializeUShort(&buf[bufPos], nodeNumID);

   // nicList
   bufPos += Serialization::serializeNicList(&buf[bufPos], nicList);
   
   // isRoot
   bufPos += Serialization::serializeBool(&buf[bufPos], isRoot);
   
   // IndirectWorkListSize
   bufPos += Serialization::serializeUInt(&buf[bufPos], indirectWorkListSize);

   // DirectWorkListSize
   bufPos += Serialization::serializeUInt(&buf[bufPos], directWorkListSize);

   // SessionCount
   bufPos += Serialization::serializeUInt(&buf[bufPos], sessionCount);
   
   // statsList
   bufPos += Serialization::serializeHighResStatsList(&buf[bufPos], statsList);

}
