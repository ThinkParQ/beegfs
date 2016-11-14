#include "MovingDirInsertMsg.h"


bool MovingDirInsertMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   { // toDirInfo
      unsigned toDirInfoLen;

      if(!this->toDirInfo.deserialize(&buf[bufPos], bufLen-bufPos, &toDirInfoLen) )
         return false;

      bufPos += toDirInfoLen;
   }
   
   { // newName
      unsigned newNameBufLen;
      unsigned newNameLen;
      const char* newNameChar;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &newNameLen, &newNameChar, &newNameBufLen) )
         return false;

      this->newName.assign(newNameChar, newNameLen);

      bufPos += newNameBufLen;
   }

   { // serialBufLen
      unsigned serialBufLenFieldLen;
      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &serialBufLen,
         &serialBufLenFieldLen) )
         return false;
   
      bufPos += serialBufLenFieldLen;
   }
   
   { // serialBuf
      serialBuf = &buf[bufPos];

      bufPos += serialBufLen;
   }

   return true;
}

void MovingDirInsertMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // toDirInfo
   bufPos += this->toDirInfoPtr->serialize(&buf[bufPos]);

   // newName
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos],
      this->newName.length(), this->newName.c_str() );

   // serialBufLen
   bufPos += Serialization::serializeUInt(&buf[bufPos], serialBufLen);
   
   // serialBuf
   memcpy(&buf[bufPos], serialBuf, serialBufLen);
   bufPos += serialBufLen;

}

