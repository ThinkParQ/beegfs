#include "RegisterNodeMsg.h"

bool RegisterNodeMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   { // instanceVersion
      unsigned instanceBufLen;

      if(!Serialization::deserializeUInt64(&buf[bufPos], bufLen-bufPos,
         &instanceVersion, &instanceBufLen) )
         return false;

      bufPos += instanceBufLen;
   }

   { // nicVersion
      unsigned nicVersionBufLen;

      if(!Serialization::deserializeUInt64(&buf[bufPos], bufLen-bufPos,
         &nicListVersion, &nicVersionBufLen) )
         return false;

      bufPos += nicVersionBufLen;
   }

   { // nodeType
      unsigned nodeTypeBufLen;

      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos,
         &nodeType, &nodeTypeBufLen) )
         return false;

      bufPos += nodeTypeBufLen;
   }

   { // fhgfsVersion
      unsigned versionBufLen;

      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos,
         &fhgfsVersion, &versionBufLen) )
         return false;

      bufPos += versionBufLen;
   }

   { // nodeNumID
      unsigned nodeNumIDBufLen;

      if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
         &nodeNumID, &nodeNumIDBufLen) )
         return false;

      bufPos += nodeNumIDBufLen;
   }

   { // rootNumID
      unsigned rootNumIDBufLen;

      if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
         &rootNumID, &rootNumIDBufLen) )
         return false;

      bufPos += rootNumIDBufLen;
   }

   { // portUDP
      unsigned portUDPBufLen;

      if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
         &portUDP, &portUDPBufLen) )
         return false;

      bufPos += portUDPBufLen;
   }

   { // portTCP
      unsigned portTCPBufLen;

      if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
         &portTCP, &portTCPBufLen) )
         return false;

      bufPos += portTCPBufLen;
   }

   { // nodeID
      unsigned nodeBufLen;

      if(!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos,
         &nodeIDLen, &nodeID, &nodeBufLen) )
         return false;

      bufPos += nodeBufLen;
   }

   { // nicList
      unsigned nicListBufLen;

      if(!Serialization::deserializeNicListPreprocess(&buf[bufPos], bufLen-bufPos,
         &nicListElemNum, &nicListStart, &nicListBufLen) )
         return false;

      bufPos += nicListBufLen;
   }

   return true;
}

void RegisterNodeMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // instanceVersion
   bufPos += Serialization::serializeUInt64(&buf[bufPos], instanceVersion);

   // nicVersion
   bufPos += Serialization::serializeUInt64(&buf[bufPos], nicListVersion);

   // nodeType
   bufPos += Serialization::serializeInt(&buf[bufPos], nodeType);

   // fhgfsVersion
   bufPos += Serialization::serializeInt(&buf[bufPos], fhgfsVersion);

   // nodeNumID
   bufPos += Serialization::serializeUShort(&buf[bufPos], nodeNumID);

   // rootNumID
   bufPos += Serialization::serializeUShort(&buf[bufPos], rootNumID);

   // portUDP
   bufPos += Serialization::serializeUShort(&buf[bufPos], portUDP);

   // portTCP
   bufPos += Serialization::serializeUShort(&buf[bufPos], portTCP);

   // nodeID
   bufPos += Serialization::serializeStr(&buf[bufPos], nodeIDLen, nodeID);

   // nicList
   bufPos += Serialization::serializeNicList(&buf[bufPos], nicList);
}

