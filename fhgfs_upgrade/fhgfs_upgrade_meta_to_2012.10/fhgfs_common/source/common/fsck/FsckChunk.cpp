#include "FsckChunk.h"

#include <common/toolkit/serialization/Serialization.h>

/**
 * Serialize into outBuf
 */
size_t FsckChunk::serialize(char* outBuf)
{
   size_t bufPos = 0;

   // id
   bufPos += Serialization::serializeStrAlign4(&outBuf[bufPos], this->getID().length(),
      this->getID().c_str());

   // targetID
   bufPos += Serialization::serializeUShort(&outBuf[bufPos], this->getTargetID());

   // filesize
   bufPos += Serialization::serializeInt64(&outBuf[bufPos], this->getFileSize());

   // creationTime
   bufPos += Serialization::serializeInt64(&outBuf[bufPos], this->getCreationTime());

   // modificationTime
   bufPos += Serialization::serializeInt64(&outBuf[bufPos], this->getModificationTime());

   // lastAccessTime
   bufPos += Serialization::serializeInt64(&outBuf[bufPos], this->getLastAccessTime());

   return bufPos;
}

/**
 * deserialize the given buffer
 */
bool FsckChunk::deserialize(const char* buf, size_t bufLen, unsigned* outLen)
{
   unsigned bufPos = 0;

   std::string id;
   uint16_t targetID;
   int64_t fileSize;
   int64_t creationTime;
   int64_t modificationTime;
   int64_t lastAccessTime;

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
      // targetID
      unsigned targetIDBufLen;

      if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos, &targetID, &targetIDBufLen))
         return false;

      bufPos += targetIDBufLen;
   }

   {
      // filesize
      unsigned filesizeBufLen;

      if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &fileSize, &filesizeBufLen))
         return false;

      bufPos += filesizeBufLen;
   }

   {
      // creationTime
      unsigned creationTimeBufLen;

      if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &creationTime,
         &creationTimeBufLen))
         return false;

      bufPos += creationTimeBufLen;
   }

   {
      // modificationTime
      unsigned modificationTimeBufLen;

      if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &modificationTime,
         &modificationTimeBufLen))
         return false;

      bufPos += modificationTimeBufLen;
   }

   {
      // lastAccessTime
      unsigned lastAccessTimeBufLen;

      if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos, &lastAccessTime,
         &lastAccessTimeBufLen))
         return false;

      bufPos += lastAccessTimeBufLen;
   }


   this->update(id, targetID, fileSize, creationTime, modificationTime, lastAccessTime);
   *outLen = bufPos;

   return true;
}

/**
 * Required size for serialization
 */
unsigned FsckChunk::serialLen()
{
   unsigned length =
      Serialization::serialLenStrAlign4(this->getID().length() )   + // id
      Serialization::serialLenUShort() + // targetID
      Serialization::serialLenInt64() + // filesize
      Serialization::serialLenInt64() + // creationTime
      Serialization::serialLenInt64() + // modificationTime
      Serialization::serialLenInt64(); // lastAccessTime

      return length;
}
