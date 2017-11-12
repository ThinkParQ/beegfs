#include "RetrieveFsIDsMsg.h"

bool RetrieveFsIDsMsg::deserializePayload(const char* buf, size_t bufLen)
{
   size_t bufPos = 0;

   {
      // hashDirNum
      unsigned hashDirNumBufLen;

      if ( !Serialization::deserializeUInt(&buf[bufPos], bufLen - bufPos, &hashDirNum,
         &hashDirNumBufLen) )
         return false;

      bufPos += hashDirNumBufLen;
   }

   {
      // currentContDirName
      unsigned currentContDirNameBufLen;

      if ( !Serialization::deserializeStr(&buf[bufPos], bufLen - bufPos, &currentContDirIDLen,
         &currentContDirID, &currentContDirNameBufLen) )
         return false;

      bufPos += currentContDirNameBufLen;
   }

   {
      // maxOutIDs
      unsigned maxOutIDsBufLen;

      if ( !Serialization::deserializeUInt(&buf[bufPos], bufLen - bufPos, &maxOutIDs,
         &maxOutIDsBufLen) )
         return false;

      bufPos += maxOutIDsBufLen;
   }

   {
      // lastHashDirOffset
      unsigned lastHashDirOffsetBufLen;

      if ( !Serialization::deserializeInt64(&buf[bufPos], bufLen - bufPos, &lastHashDirOffset,
         &lastHashDirOffsetBufLen) )
         return false;

      bufPos += lastHashDirOffsetBufLen;
   }

   {
      // lastContDirOffset
      unsigned lastContDirOffsetBufLen;

      if ( !Serialization::deserializeInt64(&buf[bufPos], bufLen - bufPos, &lastContDirOffset,
         &lastContDirOffsetBufLen) )
         return false;

      bufPos += lastContDirOffsetBufLen;
   }
   
   return true;
}

void RetrieveFsIDsMsg::serializePayload(char* buf)
{
   size_t bufPos = 0;

   // hashDirNum
   bufPos += Serialization::serializeUInt(&buf[bufPos], hashDirNum);

   // currentContDirName
   bufPos += Serialization::serializeStr(&buf[bufPos], currentContDirIDLen, currentContDirID);

   // maxOutIDs
   bufPos += Serialization::serializeUInt(&buf[bufPos], maxOutIDs);
   
   // lastHashDirOffset
   bufPos += Serialization::serializeInt64(&buf[bufPos], lastHashDirOffset);

   // lastContDirOffset
   bufPos += Serialization::serializeInt64(&buf[bufPos], lastContDirOffset);
}

