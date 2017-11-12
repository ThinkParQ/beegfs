#include "RequestStorageDataRespMsg.h"

bool RequestStorageDataRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // ID
   unsigned nodeBufLen;
   
   if (!Serialization::deserializeStr(&buf[bufPos], bufLen - bufPos, &nodeIDLen, &nodeID,
      &nodeBufLen))
      return false;

   bufPos += nodeBufLen;

   // nodeNumID
   unsigned nodeNumIDBufLen = 0;

   if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos, &nodeNumID, &nodeNumIDBufLen))
      return false;

   bufPos += nodeNumIDBufLen;

   // NicList
   unsigned nicListBufLen;
   
   if (!Serialization::deserializeNicListPreprocess(&buf[bufPos], bufLen - bufPos, &nicListElemNum,
      &nicListStart, &nicListBufLen))
      return false;

   bufPos += nicListBufLen;
   
   // IndirectWorkListSize
   unsigned IndirectWorkListSizeBufLen;
   
   if (!Serialization::deserializeUInt(&buf[bufPos], bufLen - bufPos, &indirectWorkListSize,
      &IndirectWorkListSizeBufLen))
      return false;

   bufPos += IndirectWorkListSizeBufLen;
   
   // DirectWorkListSize
   unsigned DirectWorkListSizeBufLen;
   
   if (!Serialization::deserializeUInt(&buf[bufPos], bufLen - bufPos, &directWorkListSize,
      &DirectWorkListSizeBufLen))
      return false;

   bufPos += DirectWorkListSizeBufLen;
   
   // diskSpaceTotal
   unsigned diskSpaceTotalFieldLen;
   if (!Serialization::deserializeInt64(&buf[bufPos], bufLen - bufPos, &diskSpaceTotalMB,
      &diskSpaceTotalFieldLen))
      return false;

   bufPos += diskSpaceTotalFieldLen;

   // diskSpaceFree
   unsigned diskSpaceFreeFieldLen;
   if (!Serialization::deserializeInt64(&buf[bufPos], bufLen - bufPos, &diskSpaceFreeMB,
      &diskSpaceFreeFieldLen))
      return false;

   bufPos += diskSpaceFreeFieldLen;
   
   // sessionCount
   unsigned sessionCountBufLen;
   
   if (!Serialization::deserializeUInt(&buf[bufPos], bufLen - bufPos, &sessionCount,
      &sessionCountBufLen))
      return false;

   bufPos += sessionCountBufLen;
   
   // statsList

   unsigned statsListBufLen;

   if (!Serialization::deserializeHighResStatsListPreprocess(&buf[bufPos], bufLen - bufPos,
      &statsListElemNum, &statsListStart, &statsListBufLen))
      return false;

   bufPos += statsListBufLen;

   // storageTargets

   unsigned storageTargetsBufLen;

   if (!Serialization::deserializeStorageTargetInfoListPreprocess(&buf[bufPos], bufLen - bufPos,
      &storageTargetsListStart, &storageTargetsListElemNum, &storageTargetsBufLen))
      return false;

   bufPos += storageTargetsBufLen;

   return true;
}


void RequestStorageDataRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // NodeId
   bufPos += Serialization::serializeStr(&buf[bufPos], nodeIDLen, nodeID);

   // nodeNumID
   bufPos += Serialization::serializeUShort(&buf[bufPos], nodeNumID);

   // nicList
   bufPos += Serialization::serializeNicList(&buf[bufPos], nicList);
   
   // IndirectWorkListSize
   bufPos += Serialization::serializeUInt(&buf[bufPos], indirectWorkListSize);
   
   // DirectWorkListSize
   bufPos += Serialization::serializeUInt(&buf[bufPos], directWorkListSize);
   
   // diskSpaceTotal
   bufPos += Serialization::serializeInt64(&buf[bufPos], diskSpaceTotalMB);

   // diskSpaceFree
   bufPos += Serialization::serializeInt64(&buf[bufPos], diskSpaceFreeMB);

   // sessionCount
   bufPos += Serialization::serializeUInt(&buf[bufPos], sessionCount);
   
   // statsList
   bufPos += Serialization::serializeHighResStatsList(&buf[bufPos], statsList);

   //storageTargets
   bufPos += Serialization::serializeStorageTargetInfoList(&buf[bufPos], storageTargets);
}
