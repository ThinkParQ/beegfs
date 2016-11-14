#include "UnlinkFileMsg.h"

bool UnlinkFileMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   { // parentInfo
      unsigned parentLen;

      if (!this->parentInfo.deserialize(&buf[bufPos], bufLen-bufPos, &parentLen) )
         return false;

      bufPos += parentLen;
   }

   {// delFileName

      unsigned nameBufLen;
      const char* name;
      unsigned nameLen;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &nameLen, &name, &nameBufLen) )
         return false;

      this->delFileName.assign(name, nameLen);
      bufPos += nameBufLen;
   }
   
   return true;
}

void UnlinkFileMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // parentInf
   bufPos += this->parentInfoPtr->serialize(&buf[bufPos]);
   
   // delFileName
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos],
      this->delFileName.length(), this->delFileName.c_str() );
}


