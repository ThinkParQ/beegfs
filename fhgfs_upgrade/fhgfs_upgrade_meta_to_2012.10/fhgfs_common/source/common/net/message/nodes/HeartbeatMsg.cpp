#include "HeartbeatMsg.h"

bool HeartbeatMsg::deserializePayload(const char* buf, size_t bufLen)
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
      unsigned valueBufLen;

      if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
         &nodeNumID, &valueBufLen) )
         return false;

      bufPos += valueBufLen;
   }

   { // rootNumID
      unsigned valueBufLen;

      if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
         &rootNumID, &valueBufLen) )
         return false;

      bufPos += valueBufLen;
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
   
   { // ackID
      unsigned ackBufLen;
   
      if(!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos,
         &ackIDLen, &ackID, &ackBufLen) )
         return false;
   
      bufPos += ackBufLen;
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

void HeartbeatMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // instanceVersion
   bufPos += Serialization::serializeUInt64(&buf[bufPos], instanceVersion);

   // nicVersion
   bufPos += Serialization::serializeUInt64(&buf[bufPos], nicListVersion);

   // nodeType
   bufPos += Serialization::serializeInt(&buf[bufPos], nodeType);

   // fhgfsVersion
   bufPos += Serialization::serializeUInt(&buf[bufPos], fhgfsVersion);

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

   // ackID
   bufPos += Serialization::serializeStr(&buf[bufPos], ackIDLen, ackID);

   // nicList
   bufPos += Serialization::serializeNicList(&buf[bufPos], nicList);
}

TestingEqualsRes HeartbeatMsg::testingEquals(NetMessage *msg)
{
   if ( msg->getMsgType() != this->getMsgType() )
      return TestingEqualsRes_FALSE;

   HeartbeatMsg *hbMsg = (HeartbeatMsg*) msg;

   if ( strcmp( hbMsg->getNodeID(), this->getNodeID() ) )
      return TestingEqualsRes_FALSE;

   if(hbMsg->getNodeNumID() != this->getNodeNumID() )
      return TestingEqualsRes_FALSE;

   NicAddressList nicListClone;
   hbMsg->parseNicList(&nicListClone);

   if ( nicListClone.size() != this->nicList->size() )
      return TestingEqualsRes_FALSE;

   // run only if sizes do match
   NicAddressListIter iterOrig = this->nicList->begin();
   NicAddressListIter iterClone = nicListClone.begin();

   for ( ; iterOrig != this->nicList->end(); iterOrig++, iterClone++ )
   {
      NicAddress nicAddrOrig = *iterOrig;
      NicAddress nicAddrClone = *iterClone;

      if ( (nicAddrOrig.ipAddr.s_addr != nicAddrClone.ipAddr.s_addr)
         || (nicAddrOrig.nicType != nicAddrClone.nicType) )
      {
         return TestingEqualsRes_FALSE;
      }
   }

   return TestingEqualsRes_TRUE;
}


