#include "FsckFsID.h"

#include <common/toolkit/serialization/Serialization.h>

/**
 * Serialize into outBuf
 */
size_t FsckFsID::serialize(char* outBuf)
{
   size_t bufPos = 0;

   // id
   bufPos += Serialization::serializeStrAlign4(&outBuf[bufPos], this->getID().length(),
      this->getID().c_str());

   // parentDirID
   bufPos += Serialization::serializeStrAlign4(&outBuf[bufPos], this->getParentDirID().length(),
      this->getParentDirID().c_str());

   // saveNodeID
   bufPos += Serialization::serializeUShort(&outBuf[bufPos], this->getSaveNodeID());

   // saveDevice
   bufPos += Serialization::serializeInt(&outBuf[bufPos], this->getSaveDevice());

   // saveInode
   bufPos += Serialization::serializeUInt64(&outBuf[bufPos], this->getSaveInode());

   return bufPos;
}

/**
 * deserialize the given buffer
 */
bool FsckFsID::deserialize(const char* buf, size_t bufLen, unsigned* outLen)
{
   unsigned bufPos = 0;

   std::string id;
   std::string parentDirID;
   uint16_t saveNodeID;
   int saveDevice;
   uint64_t saveInode;

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
      // saveNodeID
      unsigned saveNodeIDBufLen;

      if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos, &saveNodeID, &saveNodeIDBufLen))
         return false;

      bufPos += saveNodeIDBufLen;
   }

   {
      // saveDevice
      unsigned saveDeviceBufLen;

      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos, &saveDevice, &saveDeviceBufLen))
         return false;

      bufPos += saveDeviceBufLen;
   }

   {
      // saveInode
      unsigned saveInodeBufLen;

      if(!Serialization::deserializeUInt64(&buf[bufPos], bufLen-bufPos, &saveInode,
         &saveInodeBufLen))
         return false;

      bufPos += saveInodeBufLen;
   }

   this->update(id, parentDirID, saveNodeID, saveDevice, saveInode);
   *outLen = bufPos;

   return true;
}

/**
 * Required size for serialization
 */
unsigned FsckFsID::serialLen()
{
   unsigned length =
      Serialization::serialLenStrAlign4(this->getID().length() ) + // id
      Serialization::serialLenStrAlign4(this->getParentDirID().length() ) + // parentDirID
      Serialization::serialLenUShort() + // saveNodeID
      Serialization::serialLenInt() + // saveDevice
      Serialization::serialLenUInt64(); // saveInode

      return length;
}
