#include "FsckDirInode.h"
#include <common/toolkit/serialization/Serialization.h>

/**
 * Serialize into outBuf
 */
size_t FsckDirInode::serialize(char* outBuf)
{
   size_t bufPos = 0;

   // id
   bufPos += Serialization::serializeStrAlign4(&outBuf[bufPos], this->getID().length(),
      this->getID().c_str());

   // parentDirID
   bufPos += Serialization::serializeStrAlign4(&outBuf[bufPos], this->getParentDirID().length(),
      this->getParentDirID().c_str());

   // parentNodeID
   bufPos += Serialization::serializeUShort(&outBuf[bufPos], this->getParentNodeID());

   // ownerNodeID
   bufPos += Serialization::serializeUShort(&outBuf[bufPos], this->getOwnerNodeID());

   // size
   bufPos += Serialization::serializeInt64(&outBuf[bufPos], this->size);

   // numHardLinks
   bufPos += Serialization::serializeUInt(&outBuf[bufPos], this->numHardLinks);

   // stripeTargets
   bufPos += Serialization::serializeUInt16Vector(&outBuf[bufPos], this->getStripeTargets());

   // stripePatternType
   bufPos += Serialization::serializeInt(&outBuf[bufPos], (int) stripePatternType);

   // saveNodeID
   bufPos += Serialization::serializeUShort(&outBuf[bufPos], this->getSaveNodeID());

   // readable
   bufPos += Serialization::serializeBool(&outBuf[bufPos], this->getReadable());

   return bufPos;
}

/**
 * deserialize the given buffer
 */
bool FsckDirInode::deserialize(const char* buf, size_t bufLen, unsigned* outLen)
{
   unsigned bufPos = 0;

   std::string id;
   std::string parentDirID;
   uint16_t parentNodeID;
   uint16_t ownerNodeID;
   int64_t size;
   unsigned numHardLinks;
   UInt16Vector stripeTargets;
   int intStripePatternType;
   uint16_t saveNodeID;
   bool readable;

   {
      // id
      unsigned idBufLen;
      const char* idChar;
      unsigned idLen;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &idLen, &idChar, &idBufLen) )
         return false;

      id.assign(idChar, idLen);
      bufPos += idBufLen;
   }

   {
      // parentDirID
      unsigned parentDirIDBufLen;
      const char* parentDirIDChar;
      unsigned parentDirIDLen;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &parentDirIDLen, &parentDirIDChar, &parentDirIDBufLen) )
         return false;

      parentDirID.assign(parentDirIDChar, parentDirIDLen);
      bufPos += parentDirIDBufLen;
   }

   {
      // parentNodeID
      unsigned parentNodeIDBufLen;

      if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
         &parentNodeID, &parentNodeIDBufLen) )
         return false;

      bufPos += parentNodeIDBufLen;
   }

   {
      // ownerNodeID
      unsigned ownerNodeIDBufLen;

      if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
         &ownerNodeID, &ownerNodeIDBufLen) )
         return false;

      bufPos += ownerNodeIDBufLen;
   }

   {
      // size
      unsigned sizeBufLen;

      if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos,
         &size, &sizeBufLen) )
         return false;

      bufPos += sizeBufLen;
   }

   {
      // numHardLinks
      unsigned numHardLinksBufLen;

      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos,
         &numHardLinks, &numHardLinksBufLen) )
         return false;

      bufPos += numHardLinksBufLen;
   }

   {
      // stripeTargets
      unsigned elemNum;
      const char* vecStart;
      unsigned vecBufLen;

      if ( !Serialization::deserializeUInt16VectorPreprocess(&buf[bufPos], bufLen - bufPos,
         &elemNum, &vecStart, &vecBufLen) )
         return false;

      if ( !Serialization::deserializeUInt16Vector(vecBufLen, elemNum, vecStart, &stripeTargets) )
         return false;

      bufPos += vecBufLen;
   }

   {
      // stripePatternType
      unsigned stripePatternTypeBufLen;

      if ( !Serialization::deserializeInt(&buf[bufPos], bufLen - bufPos, &intStripePatternType,
         &stripePatternTypeBufLen) )
         return false;

      bufPos += stripePatternTypeBufLen;
   }

   {
      // saveNodeID
      unsigned saveNodeIDBufLen;

      if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
         &saveNodeID, &saveNodeIDBufLen) )
         return false;

      bufPos += saveNodeIDBufLen;
   }

   {
      // readable
      unsigned readableBufLen;

      if(!Serialization::deserializeBool(&buf[bufPos], bufLen-bufPos,
         &readable, &readableBufLen) )
         return false;

      bufPos += readableBufLen;
   }

   this->update(id, parentDirID, parentNodeID, ownerNodeID, size, numHardLinks, stripeTargets,
      stripePatternType, saveNodeID, readable);

   *outLen = bufPos;

   return true;
}

/**
 * Required size for serialization
 */
unsigned FsckDirInode::serialLen()
{
   unsigned length =
      Serialization::serialLenStrAlign4(this->getID().length() )   + // id
      Serialization::serialLenStrAlign4(this->getParentDirID().length() ) + // name
      Serialization::serialLenUShort() + // parentID
      Serialization::serialLenUShort() + // entryOwnerNodeID
      Serialization::serialLenInt64() + // size
      Serialization::serialLenUInt() + // numHardLinks
      Serialization::serialLenUInt16Vector(this->getStripeTargets()) + // stripeTargets
      Serialization::serialLenInt() + // stripePatternType
      Serialization::serialLenUShort() + // saveNodeID
      Serialization::serialLenBool(); // readable

      return length;
}
