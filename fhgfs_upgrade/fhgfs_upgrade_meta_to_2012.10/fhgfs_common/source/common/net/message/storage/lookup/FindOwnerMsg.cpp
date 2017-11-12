#include "FindOwnerMsg.h"

bool FindOwnerMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;
   
   {// searchDepth
      unsigned searchLen;

      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &searchDepth, &searchLen) )
         return false;

      bufPos += searchLen;
   }

   {// currentDepth
      unsigned currentLen;

      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos, &currentDepth, &currentLen) )
         return false;

      bufPos += currentLen;
   }

   { // entryInfo
      unsigned entryLen;

      if(!this->entryInfo.deserialize(&buf[bufPos], bufLen-bufPos, &entryLen ) )
         return false;

      bufPos += entryLen;
   }

   { // path
      unsigned pathBufLen;

      if(!Serialization::deserializePathPreprocess(&buf[bufPos], bufLen-bufPos,
         &pathDeserInfo, &pathBufLen) )
         return false;

      bufPos += pathBufLen;
   }
   
   return true;
}

void FindOwnerMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // searchDepth
   bufPos += Serialization::serializeUInt(&buf[bufPos], searchDepth);

   // currentDepth
   bufPos += Serialization::serializeUInt(&buf[bufPos], currentDepth);

   // entryInfo
   bufPos += this->entryInfoPtr->serialize(&buf[bufPos]);

   // path
   bufPos += Serialization::serializePath(&buf[bufPos], path);
}

