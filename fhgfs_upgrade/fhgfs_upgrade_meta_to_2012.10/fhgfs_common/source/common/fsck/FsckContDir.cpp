#include "FsckContDir.h"

#include <common/toolkit/serialization/Serialization.h>

/**
 * Serialize into outBuf
 */
size_t FsckContDir::serialize(char* outBuf)
{
   size_t bufPos = 0;

   // id
   bufPos += Serialization::serializeStrAlign4(&outBuf[bufPos], this->getID().length(),
      this->getID().c_str());

   // saveNodeID
   bufPos += Serialization::serializeUShort(&outBuf[bufPos], this->getSaveNodeID());

   return bufPos;
}

/**
 * deserialize the given buffer
 */
bool FsckContDir::deserialize(const char* buf, size_t bufLen, unsigned* outLen)
{
   unsigned bufPos = 0;

   std::string id;
   uint16_t saveNodeID;

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
      // saveNodeID
      unsigned saveNodeIDBufLen;

      if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
         &saveNodeID, &saveNodeIDBufLen) )
         return false;

      bufPos += saveNodeIDBufLen;
   }

   this->update(id, saveNodeID);

   *outLen = bufPos;

   return true;
}

/**
 * Required size for serialization
 */
unsigned FsckContDir::serialLen(void)
{
   unsigned length =
      Serialization::serialLenStrAlign4(this->getID().length() )   + // id
      Serialization::serialLenUShort(); // saveNodeID

      return length;
}
