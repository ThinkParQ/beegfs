#include "GetNodeInfoRespMsg.h"

bool GetNodeInfoRespMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   // nodeID

   unsigned nodeIDBufLen;

   if(!Serialization::deserializeStr(&buf[bufPos], bufLen - bufPos, &nodeIDLen, &nodeID,
      &nodeIDBufLen))
      return false;

   bufPos += nodeIDBufLen;

   // nodeNumID

   unsigned nodeNumIDBufLen;

   if(!Serialization::deserializeUShort(&buf[bufPos], bufLen - bufPos, &nodeNumID,
      &nodeNumIDBufLen))
      return false;

   bufPos += nodeNumIDBufLen;


   // GeneralNodeInfo

   // cpuName
   unsigned cpuNameBufLen;
   const char *cpuName;

   if(!Serialization::deserializeStr(&buf[bufPos], bufLen - bufPos, &cpuNameLen, &cpuName,
      &cpuNameBufLen))
      return false;
   info.cpuName = cpuName;

   bufPos += cpuNameBufLen;

   //cpuCount
   unsigned cpuCountBufLen;

   if(!Serialization::deserializeInt(&buf[bufPos], bufLen - bufPos, &(info.cpuCount),
      &cpuCountBufLen))
      return false;

   bufPos += cpuCountBufLen;

   //cpuSpeed
   unsigned cpuSpeedBufLen;

   if(!Serialization::deserializeInt(&buf[bufPos], bufLen - bufPos, &(info.cpuSpeed),
      &cpuSpeedBufLen))
      return false;

   bufPos += cpuSpeedBufLen;

   //memTotal
   unsigned memTotalBufLen;

   if(!Serialization::deserializeInt(&buf[bufPos], bufLen - bufPos, &(info.memTotal),
      &memTotalBufLen))
      return false;

   bufPos += memTotalBufLen;

   //memFree
   unsigned memFreeBufLen;

   if(!Serialization::deserializeInt(&buf[bufPos], bufLen - bufPos, &(info.memFree),
      &memFreeBufLen))
      return false;

   bufPos += memFreeBufLen;

   // logFilePath
   unsigned logFilePathBufLen;
   const char *logFilePath;

   if(!Serialization::deserializeStr(&buf[bufPos], bufLen - bufPos, &logFilePathLen, &logFilePath,
      &logFilePathBufLen))
      return false;
   info.logFilePath = logFilePath;

   bufPos += logFilePathBufLen;

   return true;
}

void GetNodeInfoRespMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // nodeID
   bufPos += Serialization::serializeStr(&buf[bufPos], nodeIDLen, nodeID);

   // nodeNumID
   bufPos += Serialization::serializeUShort(&buf[bufPos], nodeNumID);

   // info
   bufPos += Serialization::serializeStr(&buf[bufPos], cpuNameLen, (info.cpuName).c_str());

   bufPos += Serialization::serializeInt(&buf[bufPos], info.cpuCount);

   bufPos += Serialization::serializeInt(&buf[bufPos], info.cpuSpeed);

   bufPos += Serialization::serializeInt(&buf[bufPos], info.memTotal);

   bufPos += Serialization::serializeInt(&buf[bufPos], info.memFree);

   bufPos += Serialization::serializeStr(&buf[bufPos], logFilePathLen, (info.logFilePath).c_str());
}

