#include "MovingFileInsertMsg.h"


bool MovingFileInsertMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   { // fromFileInfo
      unsigned fromFileInfoBufLen;

      if(!this->fromFileInfo.deserialize(&buf[bufPos], bufLen-bufPos, &fromFileInfoBufLen) )
         return false;

      bufPos += fromFileInfoBufLen;
   }

   { // toDirInfo
      unsigned toDirBufLen;

      if(!this->toDirInfo.deserialize(&buf[bufPos], bufLen-bufPos, &toDirBufLen) )
         return false;

      bufPos += toDirBufLen;
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

   { // bufLen
      unsigned serialBufFieldLen;
      if (!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos,
         &this->serialBufLen, &serialBufFieldLen) )
         return false;

      bufPos += serialBufFieldLen;
   }

   { // serialBuf
      this->serialBuf = &buf[bufPos];

      bufPos += this->serialBufLen;
   }

   return true;
}

void MovingFileInsertMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // fromFileInfoPtr
   bufPos += this->fromFileInfoPtr->serialize(&buf[bufPos]);

   // toDirInfoPtr
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

