/*
 * Serialization of nodes
 */

#include "Serialization.h"

/**
 * Serialization of a NodeList.
 *
 * @return 0 on error, used buffer size otherwise
 */
unsigned Serialization::serializeNodeList(char* buf, NodeList* nodeList)
{
   unsigned nodeListSize = nodeList->size();

   size_t bufPos = 0;

   // elem count info field

   bufPos += serializeUInt(&buf[bufPos], nodeListSize);


   // serialize each element of the nodeList

   NodeListIter iter = nodeList->begin();

   for(unsigned i=0; i < nodeListSize; i++, iter++)
   {
      NodeConnPool* connPool = (*iter)->getConnPool();

      // nodeID
      std::string nodeID = (*iter)->getID();
      bufPos += serializeStr(&buf[bufPos], nodeID.length(), nodeID.c_str() );

      // nodeNumID
      uint16_t nodeNumID = (*iter)->getNumID();
      bufPos += serializeUShort(&buf[bufPos], nodeNumID);

      // portUDP
      uint16_t portUDP = (*iter)->getPortUDP();
      bufPos += serializeUShort(&buf[bufPos], portUDP);

      // portTCP
      uint16_t portTCP = (*iter)->getPortTCP();
      bufPos += serializeUShort(&buf[bufPos], portTCP);

      // nicList
      NicAddressList nicList(connPool->getNicList() );
      bufPos += serializeNicList(&buf[bufPos], &nicList);

      // nodeType
      char nodeType = (char) (*iter)->getNodeType();
      bufPos += serializeChar(&buf[bufPos], nodeType);

      // fhgfsVersion
      unsigned fhgfsVersion = (*iter)->getFhgfsVersion();
      bufPos += serializeUInt(&buf[bufPos], fhgfsVersion);
   }

   return bufPos;
}

/**
 * Pre-processes a serialized NodeList for deserialization via deserializeNodeList().
 *
 * @return false on error or inconsistency
 */
bool Serialization::deserializeNodeListPreprocess(const char* buf, size_t bufLen,
   unsigned* outNodeListElemNum, const char** outNodeListStart, unsigned* outLen)
{
   // Note: This is a fairly inefficient implementation (requiring redundant pre-processing),
   //    but efficiency doesn't matter for the typical use-cases of this method

   size_t bufPos = 0;

   // elem count info field
   unsigned elemNumFieldLen;

   if(unlikely(!deserializeUInt(&buf[bufPos], bufLen-bufPos, outNodeListElemNum,
      &elemNumFieldLen) ) )
      return false;

   bufPos += elemNumFieldLen;


   *outNodeListStart = &buf[bufPos];

   for(unsigned i=0; i < *outNodeListElemNum; i++)
   {
      // nodeID
      unsigned nodeIDLen = 0;
      const char* nodeID = NULL;
      unsigned idBufLen = 0;

      if(unlikely(!Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos,
         &nodeIDLen, &nodeID, &idBufLen) ) )
         return false;

      bufPos += idBufLen;

      // nodeNumID
      uint16_t nodeNumID = 0;
      unsigned nodeNumIDBufLen = 0;

      if(unlikely(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos, &nodeNumID,
         &nodeNumIDBufLen) ) )
         return false;

      bufPos += nodeNumIDBufLen;

      // portUDP
      uint16_t portUDP = 0;
      unsigned udpBufLen = 0;

      if(unlikely(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos, &portUDP,
         &udpBufLen) ) )
         return false;

      bufPos += udpBufLen;

      // portTCP
      uint16_t portTCP = 0;
      unsigned tcpBufLen = 0;

      if(unlikely(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos, &portTCP,
         &tcpBufLen) ) )
         return false;

      bufPos += tcpBufLen;

      // nicList
      unsigned nicListElemNum = 0;
      const char* nicListStart = NULL;
      unsigned nicListBufLen = 0;

      if(unlikely(!Serialization::deserializeNicListPreprocess(&buf[bufPos], bufLen-bufPos,
         &nicListElemNum, &nicListStart, &nicListBufLen) ) )
         return false;

      bufPos += nicListBufLen;

      // nodeType
      char nodeType = 0;
      unsigned nodeTypeBufLen = 0;

      if(unlikely(!Serialization::deserializeChar(&buf[bufPos], bufLen-bufPos, &nodeType,
         &nodeTypeBufLen) ) )
         return false;

      bufPos += nodeTypeBufLen;

      // fhgfsVersion
      unsigned fhgfsVersion = 0;
      unsigned versionBufLen = 0;

      if(unlikely(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &fhgfsVersion,
         &versionBufLen) ) )
         return false;

      bufPos += versionBufLen;
   }

   *outLen = bufPos;

   return true;
}

/**
 * Deserializes a NodeList.
 * (requires pre-processing)
 *
 * Note: Nodes will be constructed and need to be deleted later
 */
void Serialization::deserializeNodeList(unsigned nodeListElemNum, const char* nodeListStart,
   NodeList* outNodeList)
{
   // Note: This is a fairly inefficient implementation (containing redundant pre-processing),
   //    but efficiency doesn't matter for the typical use-cases of this method

   size_t bufPos = 0;
   size_t bufLen = ~0; // fake bufLen to max value (has already been verified during pre-processing)
   const char* buf = nodeListStart;


   for(unsigned i=0; i < nodeListElemNum; i++)
   {
      // nodeID
      unsigned nodeIDLen = 0;
      const char* nodeID = NULL;
      unsigned idBufLen = 0;

      Serialization::deserializeStr(&buf[bufPos], bufLen-bufPos, &nodeIDLen, &nodeID, &idBufLen);

      bufPos += idBufLen;

      // nodeNumID
      uint16_t nodeNumID = 0;
      unsigned nodeNumIDBufLen = 0;

      Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos, &nodeNumID, &nodeNumIDBufLen);

      bufPos += nodeNumIDBufLen;

      // portUDP
      uint16_t portUDP = 0;
      unsigned udpBufLen = 0;

      Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos, &portUDP, &udpBufLen);

      bufPos += udpBufLen;

      // portTCP
      uint16_t portTCP = 0;
      unsigned tcpBufLen = 0;

      Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos, &portTCP, &tcpBufLen);

      bufPos += tcpBufLen;

      // nicList
      unsigned nicListElemNum = 0;
      const char* nicListStart = NULL;
      unsigned nicListBufLen = 0;

      Serialization::deserializeNicListPreprocess(&buf[bufPos], bufLen-bufPos,
         &nicListElemNum, &nicListStart, &nicListBufLen);

      bufPos += nicListBufLen;

      // nodeType
      char nodeType = 0;
      unsigned nodeTypeBufLen = 0;

      Serialization::deserializeChar(&buf[bufPos], bufLen-bufPos, &nodeType, &nodeTypeBufLen);

      bufPos += nodeTypeBufLen;

      // fhgfsVersion
      unsigned fhgfsVersion = 0;
      unsigned versionBufLen = 0;

      Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &fhgfsVersion, &versionBufLen);

      bufPos += versionBufLen;


      NicAddressList nicList;
      Serialization::deserializeNicList(nicListElemNum, nicListStart, &nicList);

      // construct node
      Node* node = new Node(nodeID, nodeNumID, portUDP, portTCP, nicList);

      node->setNodeType( (NodeType)nodeType);
      node->setFhgfsVersion(fhgfsVersion);

      // append node to outList
      outNodeList->push_back(node);
   }
}

unsigned Serialization::serialLenNodeList(NodeList* nodeList)
{
   // note: this is basically serializeNodeList() (without the serialization ;)

   unsigned nodeListSize = nodeList->size();

   size_t bufPos = 0;

   // elem count info field

   bufPos += serialLenUInt();


   // serialize each element of the nodeList

   NodeListIter iter = nodeList->begin();

   for(unsigned i=0; i < nodeListSize; i++, iter++)
   {
      NodeConnPool* connPool = (*iter)->getConnPool();

      // nodeID
      std::string nodeID = (*iter)->getID();
      bufPos += serialLenStr(nodeID.length() );

      // nodeNumID
      bufPos += serialLenUShort();

      // portUDP
      bufPos += serialLenUShort();

      // portTCP
      bufPos += serialLenUShort();

      // nicList
      NicAddressList nicList(connPool->getNicList() );
      bufPos += serialLenNicList(&nicList);

      // nodeType
      bufPos += serialLenChar();

      // fhgfsVersion
      bufPos += serialLenUInt();
   }

   return bufPos;
}
