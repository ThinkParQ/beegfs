#include "RmDirMsg.h"


bool RmDirMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   { // parentInfo
      unsigned entryBufLen;

      if(!this->parentInfo.deserialize(&buf[bufPos], bufLen-bufPos, &entryBufLen) )
         return false;

      bufPos += entryBufLen;
   }

   { // delDirName
      unsigned nameBufLen;
      const char* delDirNameChar;
      unsigned delDirNameLen;

      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &delDirNameLen, &delDirNameChar, &nameBufLen) )
      return false;

      this->delDirName.assign(delDirNameChar, delDirNameLen);
      bufPos += nameBufLen; // not needed right now. included to avoid human errors later ;)
   }
   
   return true;
}

void RmDirMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   bufPos += this->parentInfoPtr->serialize(&buf[bufPos]);
   
   // delDirName
   bufPos += Serialization::serializeStrAlign4(&buf[bufPos],
      this->delDirName.length(), this->delDirName.c_str() );
}

