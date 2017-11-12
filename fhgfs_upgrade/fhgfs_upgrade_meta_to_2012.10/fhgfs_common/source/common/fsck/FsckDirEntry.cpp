#include "FsckDirEntry.h"

#include <common/toolkit/serialization/Serialization.h>

/**
 * Serialize into outBuf
 */
size_t FsckDirEntry::serialize(char* outBuf)
{
   size_t bufPos = 0;

   // id
   bufPos += Serialization::serializeStrAlign4(&outBuf[bufPos], this->getID().length(),
      this->getID().c_str());

   // name
   bufPos += Serialization::serializeStrAlign4(&outBuf[bufPos], this->getName().length(),
      this->getName().c_str());

   // parentID
   bufPos += Serialization::serializeStrAlign4(&outBuf[bufPos], this->getParentDirID().length(),
      this->getParentDirID().c_str());

   // entryOwnerNodeID
   bufPos += Serialization::serializeUShort(&outBuf[bufPos], this->getEntryOwnerNodeID());

   // inodeOwnerNodeID
   bufPos += Serialization::serializeUShort(&outBuf[bufPos], this->getInodeOwnerNodeID());

   // entryType
   bufPos += Serialization::serializeInt(&outBuf[bufPos], (int)this->getEntryType());

   // saveNodeID
   bufPos += Serialization::serializeUShort(&outBuf[bufPos], this->getSaveNodeID());

   // saveDevice
   bufPos += Serialization::serializeInt(&outBuf[bufPos], (int)this->getSaveDevice());

   // saveInode
   bufPos += Serialization::serializeUInt64(&outBuf[bufPos], (uint64_t)this->getSaveInode());

   return bufPos;
}

/**
 * deserialize the given buffer
 */
bool FsckDirEntry::deserialize(const char* buf, size_t bufLen, unsigned* outLen)
{
   unsigned bufPos = 0;

   std::string id;
   std::string name;
   std::string parentID;
   uint16_t entryOwnerNodeID;
   uint16_t inodeOwnerNodeID;
   int intEntryType;
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
      // name
      unsigned nameBufLen;
      const char* nameChar;
      unsigned nameLen;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &nameLen, &nameChar, &nameBufLen) )
         return false;

      name.assign(nameChar, nameLen);

      bufPos += nameBufLen;
   }

   {
      // parentID
      unsigned parentIDBufLen;
      const char* parentIDChar;
      unsigned parentIDLen;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &parentIDLen, &parentIDChar, &parentIDBufLen) )
         return false;

      parentID.assign(parentIDChar, parentIDLen);

      bufPos += parentIDBufLen;
   }

   {
      // entryOwnerNodeID
      unsigned entryOwnerNodeIDBufLen;

      if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
         &entryOwnerNodeID, &entryOwnerNodeIDBufLen) )
         return false;

      bufPos += entryOwnerNodeIDBufLen;
   }


   {
      // inodeOwnerNodeID
      unsigned inodeOwnerNodeIDBufLen;

      if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
         &inodeOwnerNodeID, &inodeOwnerNodeIDBufLen) )
         return false;

      bufPos += inodeOwnerNodeIDBufLen;
   }


   {
      // entryType
      unsigned entryTypeBufLen;

      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos,
         &intEntryType, &entryTypeBufLen) )
         return false;

      bufPos += entryTypeBufLen;
   }

   {
      // saveNodeID
      unsigned saveNodeIDBufLen;

      if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
         &saveNodeID, &saveNodeIDBufLen) )
         return false;

      bufPos += saveNodeIDBufLen;
   }

   // saveDevice
   {
      unsigned saveDeviceBufLen;

      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos,
         &saveDevice, &saveDeviceBufLen) )
         return false;

      bufPos += saveDeviceBufLen;
   }

   // saveInode
   {
      unsigned saveInodeBufLen;

      if(!Serialization::deserializeUInt64(&buf[bufPos], bufLen-bufPos,
         &saveInode, &saveInodeBufLen) )
         return false;

      bufPos += saveInodeBufLen;
   }

   this->update(id, name, parentID, entryOwnerNodeID, inodeOwnerNodeID,
      (FsckDirEntryType)intEntryType, saveNodeID, saveDevice, saveInode);

   *outLen = bufPos;

   return true;
}

/**
 * Required size for serialization
 */
unsigned FsckDirEntry::serialLen(void)
{
   unsigned length =
      Serialization::serialLenStrAlign4(this->getID().length() )   + // id
      Serialization::serialLenStrAlign4(this->getName().length() ) + // name
      Serialization::serialLenStrAlign4(this->getParentDirID().length() ) + // parentID
      Serialization::serialLenUShort() + // entryOwnerNodeID
      Serialization::serialLenUShort() + // inodeOwnerNodeID
      Serialization::serialLenInt() + // entryType
      Serialization::serialLenUShort() + // saveNodeID
      Serialization::serialLenInt() + // saveDevice
      Serialization::serialLenUInt64(); // saveInode

      return length;
}
