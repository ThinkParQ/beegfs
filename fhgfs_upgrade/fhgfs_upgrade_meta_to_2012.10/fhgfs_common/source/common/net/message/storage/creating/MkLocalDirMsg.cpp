#include "MkLocalDirMsg.h"


bool MkLocalDirMsg::deserializePayload(const char* buf, size_t bufLen)
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

   { // entryInfo
      unsigned entryBufLen;
      if (!entryInfo.deserialize(&buf[bufPos], bufLen-bufPos, &entryBufLen) )
         return false;

      bufPos += entryBufLen;
   }

   { // stripePattern
      unsigned patternBufLen;
      if(!StripePattern::deserializePatternPreprocess(&buf[bufPos], bufLen-bufPos,
         &patternHeader, &patternStart, &patternBufLen) )
         return false;

      bufPos += patternBufLen;
   }

   { // parentNodeID
      unsigned parentNodeBufLen;

      if(!Serialization::deserializeUShort(&buf[bufPos], bufLen-bufPos,
         &parentNodeID, &parentNodeBufLen) )
         return false;

      bufPos += parentNodeBufLen;
   }

   return true;
}

void MkLocalDirMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // userID
   bufPos += Serialization::serializeUInt(&buf[bufPos], userID);
   
   // groupID
   bufPos += Serialization::serializeUInt(&buf[bufPos], groupID);

   // mode
   bufPos += Serialization::serializeInt(&buf[bufPos], mode);

   // entryInfo
   bufPos += entryInfoPtr->serialize(&buf[bufPos]);

   // stripePattern
   bufPos += pattern->serialize(&buf[bufPos]);

   // parentNodeID
   bufPos += Serialization::serializeUShort(&buf[bufPos], parentNodeID);
}

