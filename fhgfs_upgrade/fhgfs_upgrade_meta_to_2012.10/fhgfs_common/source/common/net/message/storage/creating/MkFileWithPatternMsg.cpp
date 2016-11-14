#include "MkFileWithPatternMsg.h"


bool MkFileWithPatternMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   { // userID
      unsigned userIDLen;
      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &userID, &userIDLen) )
         return false;

      bufPos += userIDLen;
   }
   
   
   { // groupID
      unsigned groupIDLen;
      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &groupID, &groupIDLen) )
         return false;

      bufPos += groupIDLen;
   }

   { // mode
      unsigned modeLen;
      if(!Serialization::deserializeInt(&buf[bufPos], bufLen-bufPos, &mode, &modeLen) )
         return false;

      bufPos += modeLen;
   }

   { // parentInfo
      unsigned parentLen;
      if (!this->parentInfo.deserialize(&buf[bufPos], bufLen-bufPos, &parentLen) )
         return false;

      bufPos += parentLen;
   }

   { // newDirName
      unsigned nameLen;
      if(!Serialization::deserializeStrAlign4(&buf[bufPos], bufLen-bufPos,
         &this->newFileNameLen, &this->newFileName, &nameLen) )
         return false;

      bufPos += nameLen;
   }

   { // stripePattern
      unsigned patternBufLen;
      if(!StripePattern::deserializePatternPreprocess(&buf[bufPos], bufLen-bufPos,
         &patternHeader, &patternStart, &patternBufLen) )
         return false;

      bufPos += patternBufLen;
   }

   return true;
}

void MkFileWithPatternMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // userID
   
   bufPos += Serialization::serializeUInt(&buf[bufPos], this->userID);
   
   // groupID
   
   bufPos += Serialization::serializeUInt(&buf[bufPos], this->groupID);

   // mode
   
   bufPos += Serialization::serializeInt(&buf[bufPos], this->mode);

   // parentInfo

   bufPos += this->parentInfoPtr->serialize(&buf[bufPos]);

   // newDirName

   bufPos += Serialization::serializeStrAlign4(&buf[bufPos], this->newFileNameLen,
      this->newFileName);

   // stripePattern
   
   bufPos += pattern->serialize(&buf[bufPos]);
}


