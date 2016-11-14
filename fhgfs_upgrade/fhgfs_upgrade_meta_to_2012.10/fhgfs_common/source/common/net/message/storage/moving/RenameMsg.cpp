#include "RenameMsg.h"


bool RenameMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   { // entryType
     unsigned entryTypeBufLen;
     unsigned unsignedEntryType;

     if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &unsignedEntryType,
        &entryTypeBufLen) )
        return false;

     this->entryType = (DirEntryType) unsignedEntryType;
     bufPos += entryTypeBufLen;
   }

   { // fromDirInfo
      unsigned fromBufLen;

      if(!this->fromDirInfo.deserialize(&buf[bufPos], bufLen-bufPos, &fromBufLen) )
         return false;

      bufPos += fromBufLen;
   }

   { // toDirInfo
      unsigned toBufLen;

      if (!this->toDirInfo.deserialize(&buf[bufPos], bufLen-bufPos, &toBufLen) )
         return false;

      bufPos += toBufLen;
   }

   { // oldName

      unsigned oldBufLen;
      const char* oldNameChar;
      unsigned oldNameLen;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &oldNameLen, &oldNameChar, &oldBufLen) )
         return false;


      this->oldName.assign(oldNameChar, oldNameLen);
      bufPos += oldBufLen;
   }

   { // newName

      unsigned newBufLen;
      const char* newNameChar;
      unsigned newNameLen;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &newNameLen, &newNameChar, &newBufLen) )
         return false;

      this->newName.assign(newNameChar, newNameLen);
      bufPos += newBufLen;
   }

   return true;
}

void RenameMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // entryType
   bufPos += Serialization::serializeUInt(&buf[bufPos], (unsigned) this->entryType);

   // fromDirInfo
   bufPos += this->fromDirInfoPtr->serialize(&buf[bufPos]);

   // toDirInfo
   bufPos += this->toDirInfoPtr->serialize(&buf[bufPos]);

   // oldName
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos],
      this->oldName.length(), this->oldName.c_str() );
   
   // newName
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos],
      this->newName.length(), this->newName.c_str() );

}

