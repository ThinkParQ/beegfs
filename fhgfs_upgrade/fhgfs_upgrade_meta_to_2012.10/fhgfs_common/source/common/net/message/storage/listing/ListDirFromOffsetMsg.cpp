#include "ListDirFromOffsetMsg.h"

bool ListDirFromOffsetMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   { // serverOffset
      unsigned serverOffsetBufLen;
   
      if(!Serialization::deserializeInt64(&buf[bufPos], bufLen-bufPos,
         &serverOffset, &serverOffsetBufLen) )
         return false;

      bufPos += serverOffsetBufLen;
   }


   {  // incrementalOffset
      unsigned incOffsetBufLen;

      if(!Serialization::deserializeUInt64(&buf[bufPos], bufLen-bufPos,
         &incrementalOffset, &incOffsetBufLen) )
         return false;

      bufPos += incOffsetBufLen;
   }


   { // maxOutNames
      unsigned maxOutNamesBufLen;

      if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos,
         &maxOutNames, &maxOutNamesBufLen) )
         return false;

      bufPos += maxOutNamesBufLen;
   }
   

   { // entryInfo
      unsigned entryInfoBufLen;

      if(!this->entryInfo.deserialize(&buf[bufPos], bufLen-bufPos, &entryInfoBufLen) )
         return false;

      bufPos += entryInfoBufLen;
   }

   return true;
}

void ListDirFromOffsetMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;
   
   // serverOffset
   bufPos += Serialization::serializeInt64(&buf[bufPos], serverOffset);
   
   // incrementalOffset
   bufPos += Serialization::serializeUInt64(&buf[bufPos], incrementalOffset);

   // maxOutNames
   bufPos += Serialization::serializeUInt(&buf[bufPos], maxOutNames);

   // entryInfo
   bufPos += this->entryInfoPtr->serialize(&buf[bufPos]);
}

